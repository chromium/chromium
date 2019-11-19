// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/hang_monitor/hang_crash_dump.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/hung_plugin_infobar_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"

namespace {

// Called on the I/O thread to actually kill the plugin with the given child
// ID. We specifically don't want this to be a member function since if the
// user chooses to kill the plugin, we want to kill it even if they close the
// tab first.
//
// Be careful with the child_id. It's supplied by the renderer which might be
// hacked.
void KillPluginOnIOThread(int child_id) {
  content::BrowserChildProcessHostIterator iter(
      content::PROCESS_TYPE_PPAPI_PLUGIN);
  while (!iter.Done()) {
    const content::ChildProcessData& data = iter.GetData();
    if (data.id == child_id) {
      CrashDumpHungChildProcess(data.GetProcess().Handle());
      data.GetProcess().Terminate(content::RESULT_CODE_HUNG, false);
      return;
    }
    ++iter;
  }
  // Ignore the case where we didn't find the plugin, it may have terminated
  // before this function could run.
}

}  // namespace

// HungPluginTabHelper::PluginState -------------------------------------------

// Per-plugin state (since there could be more than one plugin hung).  The
// integer key is the child process ID of the plugin process.  This maintains
// the state for all plugins on this page that are currently hung, whether or
// not we're currently showing the infobar.
struct HungPluginTabHelper::PluginState {
  // Initializes the plugin state to be a hung plugin.
  PluginState(const base::FilePath& path, const base::string16& name);

  // Since the scope of the timer manages our callback, this struct should
  // not be copied.
  PluginState(const PluginState&) = delete;
  PluginState& operator=(const PluginState&) = delete;

  ~PluginState() = default;

  base::FilePath path;
  base::string16 name;

  // Possibly-null if we're not showing an infobar right now.
  infobars::InfoBar* infobar = nullptr;

  // Time to delay before re-showing the infobar for a hung plugin. This is
  // increased each time the user cancels it.
  base::TimeDelta next_reshow_delay = base::TimeDelta::FromSeconds(10);

  // Handles calling the helper when the infobar should be re-shown.
  base::OneShotTimer timer;
};

HungPluginTabHelper::PluginState::PluginState(const base::FilePath& path,
                                              const base::string16& name)
    : path(path), name(name) {}

// HungPluginTabHelper --------------------------------------------------------

HungPluginTabHelper::~HungPluginTabHelper() = default;

void HungPluginTabHelper::PluginCrashed(const base::FilePath& plugin_path,
                                        base::ProcessId plugin_pid) {
  // For now, just do a brute-force search to see if we have this plugin. Since
  // we'll normally have 0 or 1, this is fast.
  const auto i = std::find_if(hung_plugins_.begin(), hung_plugins_.end(),
                              [plugin_path](const auto& elem) {
                                return elem.second->path == plugin_path;
                              });
  if (i != hung_plugins_.end()) {
    if (i->second->infobar) {
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(web_contents());
      if (infobar_service)
        infobar_service->RemoveInfoBar(i->second->infobar);
    }
    hung_plugins_.erase(i);
  }
}

void HungPluginTabHelper::PluginHungStatusChanged(
    int plugin_child_id,
    const base::FilePath& plugin_path,
    bool is_hung) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());

  auto found = hung_plugins_.find(plugin_child_id);
  if (found != hung_plugins_.end()) {
    if (!is_hung) {
      // Hung plugin became un-hung, close the infobar and delete our info.
      if (found->second->infobar && infobar_service)
        infobar_service->RemoveInfoBar(found->second->infobar);
      hung_plugins_.erase(found);
    }
    return;
  }

  if (!infobar_service)
    return;
  if (!infobar_observer_.IsObserving(infobar_service))
    infobar_observer_.Add(infobar_service);

  base::string16 plugin_name =
      content::PluginService::GetInstance()->GetPluginDisplayNameByPath(
          plugin_path);
  hung_plugins_[plugin_child_id] =
      std::make_unique<PluginState>(plugin_path, plugin_name);
  ShowBar(plugin_child_id, hung_plugins_[plugin_child_id].get());
}

void HungPluginTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                           bool animate) {
  const auto i = std::find_if(
      hung_plugins_.begin(), hung_plugins_.end(),
      [infobar](const auto& elem) { return elem.second->infobar == infobar; });
  if (i != hung_plugins_.end()) {
    PluginState* state = i->second.get();
    state->infobar = nullptr;

    // Schedule the timer to re-show the infobar if the plugin continues to be
    // hung.
    state->timer.Start(FROM_HERE, state->next_reshow_delay,
                       base::BindOnce(&HungPluginTabHelper::OnReshowTimer,
                                      base::Unretained(this), i->first));

    // Next time we do this, delay it twice as long to avoid being annoying.
    state->next_reshow_delay *= 2;
  }
}

void HungPluginTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  infobar_observer_.Remove(manager);
}

void HungPluginTabHelper::KillPlugin(int child_id) {
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&KillPluginOnIOThread, child_id));
}

HungPluginTabHelper::HungPluginTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

void HungPluginTabHelper::OnReshowTimer(int child_id) {
  // The timer should have been cancelled if the record isn't in our map
  // anymore.
  auto found = hung_plugins_.find(child_id);
  DCHECK(found != hung_plugins_.end());
  DCHECK(!found->second->infobar);
  ShowBar(child_id, found->second.get());
}

void HungPluginTabHelper::ShowBar(int child_id, PluginState* state) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  DCHECK(!state->infobar);
  state->infobar = HungPluginInfoBarDelegate::Create(infobar_service, this,
                                                     child_id, state->name);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HungPluginTabHelper)
