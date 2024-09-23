// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/browser/hang_monitor/hang_crash_dump.h"
#include "chrome/browser/plugins/hung_plugin_infobar_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"

// HungPluginTabHelper::PluginState -------------------------------------------

// Per-plugin state (since there could be more than one plugin hung).  The
// integer key is the child process ID of the plugin process.  This maintains
// the state for all plugins on this page that are currently hung, whether or
// not we're currently showing the infobar.
struct HungPluginTabHelper::PluginState {
  // Initializes the plugin state to be a hung plugin.
  PluginState(const base::FilePath& path, const std::u16string& name);

  // Since the scope of the timer manages our callback, this struct should
  // not be copied.
  PluginState(const PluginState&) = delete;
  PluginState& operator=(const PluginState&) = delete;

  ~PluginState() = default;

  base::FilePath path;
  std::u16string name;

  // Possibly-null if we're not showing an infobar right now.
  raw_ptr<infobars::InfoBar> infobar = nullptr;

  // Time to delay before re-showing the infobar for a hung plugin. This is
  // increased each time the user cancels it.
  base::TimeDelta next_reshow_delay = base::Seconds(10);

  // Handles calling the helper when the infobar should be re-shown.
  base::OneShotTimer timer;
};

HungPluginTabHelper::PluginState::PluginState(const base::FilePath& path,
                                              const std::u16string& name)
    : path(path), name(name) {}

// HungPluginTabHelper --------------------------------------------------------

HungPluginTabHelper::~HungPluginTabHelper() = default;

void HungPluginTabHelper::PluginCrashed(const base::FilePath& plugin_path,
                                        base::ProcessId plugin_pid) {
  // For now, just do a brute-force search to see if we have this plugin. Since
  // we'll normally have 0 or 1, this is fast.
  const auto i =
      base::ranges::find(hung_plugins_, plugin_path,
                         [](const auto& elem) { return elem.second->path; });
  if (i != hung_plugins_.end()) {
    if (i->second->infobar) {
      infobars::ContentInfoBarManager* infobar_manager =
          infobars::ContentInfoBarManager::FromWebContents(web_contents());
      if (infobar_manager)
        infobar_manager->RemoveInfoBar(i->second->infobar);
    }
    hung_plugins_.erase(i);
  }
}

void HungPluginTabHelper::PluginHungStatusChanged(
    int plugin_child_id,
    const base::FilePath& plugin_path,
    bool is_hung) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());

  auto found = hung_plugins_.find(plugin_child_id);
  if (found != hung_plugins_.end()) {
    if (!is_hung) {
      // Hung plugin became un-hung, close the infobar and delete our info.
      if (found->second->infobar && infobar_manager)
        infobar_manager->RemoveInfoBar(found->second->infobar);
      hung_plugins_.erase(found);
    }
    return;
  }

  if (!infobar_manager)
    return;
  if (!infobar_observations_.IsObservingSource(infobar_manager))
    infobar_observations_.AddObservation(infobar_manager);

  std::u16string plugin_name =
      content::PluginService::GetInstance()->GetPluginDisplayNameByPath(
          plugin_path);
  hung_plugins_[plugin_child_id] =
      std::make_unique<PluginState>(plugin_path, plugin_name);
  ShowBar(plugin_child_id, hung_plugins_[plugin_child_id].get());
}

void HungPluginTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                           bool animate) {
  const auto i =
      base::ranges::find(hung_plugins_, infobar,
                         [](const auto& elem) { return elem.second->infobar; });
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
  infobar_observations_.RemoveObservation(manager);
}

void HungPluginTabHelper::KillPlugin(int child_id) {
  // Be careful with the child_id. It's supplied by the renderer which might be
  // hacked.
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

HungPluginTabHelper::HungPluginTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<HungPluginTabHelper>(*contents) {}

void HungPluginTabHelper::OnReshowTimer(int child_id) {
  // The timer should have been cancelled if the record isn't in our map
  // anymore.
  auto found = hung_plugins_.find(child_id);
  CHECK(found != hung_plugins_.end(), base::NotFatalUntil::M130);
  DCHECK(!found->second->infobar);
  ShowBar(child_id, found->second.get());
}

void HungPluginTabHelper::ShowBar(int child_id, PluginState* state) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  if (!infobar_manager)
    return;

  DCHECK(!state->infobar);
  state->infobar = HungPluginInfoBarDelegate::Create(infobar_manager, this,
                                                     child_id, state->name);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(HungPluginTabHelper);
