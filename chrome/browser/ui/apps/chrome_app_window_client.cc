// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_client.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/devtools_agent_host.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#endif

// TODO(jamescook): We probably shouldn't compile this class at all on Android.
// See http://crbug.com/343612
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#endif

ChromeAppWindowClient::ChromeAppWindowClient() = default;

ChromeAppWindowClient::~ChromeAppWindowClient() = default;

// static
ChromeAppWindowClient* ChromeAppWindowClient::GetInstance() {
  return base::Singleton<
      ChromeAppWindowClient,
      base::LeakySingletonTraits<ChromeAppWindowClient>>::get();
}

extensions::AppWindow* ChromeAppWindowClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
#if BUILDFLAG(IS_ANDROID)
  return NULL;
#else
  Profile* profile = Profile::FromBrowserContext(context);
  return new extensions::AppWindow(
      context, std::make_unique<ChromeAppDelegate>(profile, true), extension);
#endif
}

extensions::AppWindow*
ChromeAppWindowClient::CreateAppWindowForLockScreenAction(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::api::app_runtime::ActionType action) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto app_delegate = std::make_unique<ChromeAppDelegate>(
      Profile::FromBrowserContext(context), true /*keep_alive*/);
  app_delegate->set_for_lock_screen_app(true);

  return lock_screen_apps::StateController::Get()
      ->CreateAppWindowForLockScreenAction(context, extension, action,
                                           std::move(app_delegate));
#else
  return nullptr;
#endif
}

std::unique_ptr<extensions::NativeAppWindow>
ChromeAppWindowClient::CreateNativeAppWindow(
    extensions::AppWindow* window,
    extensions::AppWindow::CreateParams* params) {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  return base::WrapUnique(CreateNativeAppWindowImpl(window, *params));
#endif
}

void ChromeAppWindowClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    base::OnceClosure callback) {
  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents));
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kTargetReload);

  DevToolsWindow* devtools_window =
      DevToolsWindow::FindDevToolsWindow(agent.get());
  if (devtools_window)
    devtools_window->SetLoadCompletedCallback(std::move(callback));
  else
    std::move(callback).Run();
}

bool ChromeAppWindowClient::IsCurrentChannelOlderThanDev() {
  return extensions::GetCurrentChannel() > version_info::Channel::DEV;
}
