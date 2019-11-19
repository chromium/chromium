// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings_utils.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/process/launch.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::Referrer;

namespace {

// Command used to configure GNOME 2 proxy settings.
const char* const kGNOME2ProxyConfigCommand[] = {"gnome-network-properties",
                                                 nullptr};
// In GNOME 3, we might need to run gnome-control-center instead. We try this
// only after gnome-network-properties is not found, because older GNOME also
// has this but it doesn't do the same thing. See below where we use it.
const char* const kGNOME3ProxyConfigCommand[] = {"gnome-control-center",
                                                 "network", nullptr};
// KDE3, 4, and 5 are only slightly different, but incompatible. Go figure.
const char* const kKDE3ProxyConfigCommand[] = {"kcmshell", "proxy", nullptr};
const char* const kKDE4ProxyConfigCommand[] = {"kcmshell4", "proxy", nullptr};
const char* const kKDE5ProxyConfigCommand[] = {"kcmshell5", "proxy", nullptr};

// The URL for Linux proxy configuration help when not running under a
// supported desktop environment.
constexpr char kLinuxProxyConfigUrl[] = "about:linux-proxy-config";

// Show the proxy config URL in the given tab.
void ShowLinuxProxyConfigUrl(int render_process_id, int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const char* name = base::nix::GetDesktopEnvironmentName(env.get());
  if (name)
    LOG(ERROR) << "Could not find " << name << " network settings in $PATH";
  OpenURLParams params(GURL(kLinuxProxyConfigUrl), Referrer(),
                       WindowOpenDisposition::NEW_FOREGROUND_TAB,
                       ui::PAGE_TRANSITION_LINK, false);

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (web_contents)
    web_contents->OpenURL(params);
}

// Start the given proxy configuration utility.
bool StartProxyConfigUtil(const char* const command[]) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // base::LaunchProcess() returns true ("success") if the fork()
  // succeeds, but not necessarily the exec(). We'd like to be able to
  // use StartProxyConfigUtil() to search possible options and stop on
  // success, so we search $PATH first to predict whether the exec is
  // expected to succeed.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!base::ExecutableExistsInPath(env.get(), command[0]))
    return false;

  std::vector<std::string> argv;
  for (size_t i = 0; command[i]; ++i)
    argv.push_back(command[i]);
  base::Process process = base::LaunchProcess(argv, base::LaunchOptions());
  if (!process.IsValid()) {
    LOG(ERROR) << "StartProxyConfigUtil failed to start " << command[0];
    return false;
  }
  base::EnsureProcessGetsReaped(std::move(process));
  return true;
}

// Detect, and if possible, start the appropriate proxy config utility. On
// failure to do so, show the Linux proxy config URL in a new tab instead.
void DetectAndStartProxyConfigUtil(int render_process_id,
                                   int render_view_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  bool launched = false;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY: {
      launched = StartProxyConfigUtil(kGNOME2ProxyConfigCommand);
      if (!launched) {
        // We try this second, even though it's the newer way, because this
        // command existed in older versions of GNOME, but it didn't do the
        // same thing. The older command is gone though, so this should do
        // the right thing. (Also some distributions have blurred the lines
        // between GNOME 2 and 3, so we can't necessarily detect what the
        // right thing is based on indications of which version we have.)
        launched = StartProxyConfigUtil(kGNOME3ProxyConfigCommand);
      }
      break;
    }

    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
      launched = StartProxyConfigUtil(kKDE3ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      launched = StartProxyConfigUtil(kKDE4ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
      launched = StartProxyConfigUtil(kKDE5ProxyConfigCommand);
      break;

    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
  }

  if (launched)
    return;
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&ShowLinuxProxyConfigUrl, render_process_id,
                                render_view_id));
}

}  // namespace

namespace settings_utils {

void ShowNetworkProxySettings(content::WebContents* web_contents) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&DetectAndStartProxyConfigUtil,
                     web_contents->GetRenderViewHost()->GetProcess()->GetID(),
                     web_contents->GetRenderViewHost()->GetRoutingID()));
}

}  // namespace settings_utils
