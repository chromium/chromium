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
      CrashDumpHungChildProcess(data.GetHandle());
      base::Process process =
          base::Process::DeprecatedGetProcessFromHandle(data.GetHandle());
      process.Terminate(content::RESULT_CODE_HUNG, false);
      break;
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
  PluginState(const base::FilePath& p, const base::string16& n);
  ~PluginState();

  base::FilePath path;
  base::string16 name;

  // Possibly-null if we're not showing an infobar right now.
  infobars::InfoBar* infobar;

  // Time to delay before re-showing the infobar for a hung plugin. This is
  // increased each time the user cancels it.
  base::TimeDelta next_reshow_delay;

  // Handles calling the helper when the infobar should be re-shown.
  base::OneShotTimer timer;

 private:
  // Initial delay in seconds before re-showing the hung plugin message.
  static const int kInitialReshowDelaySec;

  // Since the scope of the timer manages our callback, this struct should
  // not be copied.
  DISALLOW_COPY_AND_ASSIGN(PluginState);
};

// static
const int HungPluginTabHelper::PluginState::kInitialReshowDelaySec = 10;

HungPluginTabHelper::PluginState::PluginState(const base::FilePath& p,
                                              const base::string16& n)
    : path(p),
      name(n),
      infobar(NULL),
      next_reshow_delay(base::TimeDelta::FromSeconds(kInitialReshowDelaySec)) {}

HungPluginTabHelper::PluginState::~PluginState() {
}


// HungPluginTabHelper --------------------------------------------------------

HungPluginTabHelper::HungPluginTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents), infobar_observer_(this) {}

HungPluginTabHelper::~HungPluginTabHelper() {
}

void HungPluginTabHelper::PluginCrashed(const base::FilePath& plugin_path,
                                        base::ProcessId plugin_pid) {
  // TODO(brettw) ideally this would take the child process ID. When we do this
  // for NaCl plugins, we'll want to know exactly which process it was since
  // the path won't be useful.
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;

  // For now, just do a brute-force search to see if we have this plugin. Since
  // we'll normally have 0 or 1, this is fast.
  for (auto i = hung_plugins_.begin(); i != hung_plugins_.end(); ++i) {
    if (i->second->path == plugin_path) {
      if (i->second->infobar)
        infobar_service->RemoveInfoBar(i->second->infobar);
      hung_plugins_.erase(i);
      break;
    }
  }
}

void HungPluginTabHelper::PluginHungStatusChanged(
    int plugin_child_id,
    const base::FilePath& plugin_path,
    bool is_hung) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (!infobar_service)
    return;
  if (!infobar_observer_.IsObserving(infobar_service))
    infobar_observer_.Add(infobar_service);

  auto found = hung_plugins_.find(plugin_child_id);
  if (found != hung_plugins_.end()) {
    if (!is_hung) {
      // Hung plugin became un-hung, close the infobar and delete our info.
      if (found->second->infobar)
        infobar_service->RemoveInfoBar(found->second->infobar);
      hung_plugins_.erase(found);
    }
    return;
  }

  base::string16 plugin_name =
      content::PluginService::GetInstance()->GetPluginDisplayNameByPath(
          plugin_path);

  linked_ptr<PluginState> state(new PluginState(plugin_path, plugin_name));
  hung_plugins_[plugin_child_id] = state;
  ShowBar(plugin_child_id, state.get());
}

void HungPluginTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                           bool animate) {
  for (auto i = hung_plugins_.begin(); i != hung_plugins_.end(); ++i) {
    PluginState* state = i->second.get();
    if (state->infobar == infobar) {
      state->infobar = nullptr;

      // Schedule the timer to re-show the infobar if the plugin continues to be
      // hung.
      state->timer.Start(FROM_HERE, state->next_reshow_delay,
          base::Bind(&HungPluginTabHelper::OnReshowTimer,
                     base::Unretained(this),
                     i->first));

      // Next time we do this, delay it twice as long to avoid being annoying.
      state->next_reshow_delay *= 2;
      return;
    }
  }
}

void HungPluginTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  infobar_observer_.Remove(manager);
}

void HungPluginTabHelper::KillPlugin(int child_id) {
  auto found = hung_plugins_.find(child_id);
  DCHECK(found != hung_plugins_.end());

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                           base::BindOnce(&KillPluginOnIOThread, child_id));
  CloseBar(found->second.get());
}

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

void HungPluginTabHelper::CloseBar(PluginState* state) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  if (infobar_service && state->infobar) {
    infobar_service->RemoveInfoBar(state->infobar);
    state->infobar = NULL;
  }
}
