// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui_android.h"
#include "chrome/browser/ui/extensions/extension_install_ui_desktop.h"

namespace {

static bool g_disable_ui_for_tests = false;

}  // namespace

// static
std::unique_ptr<ExtensionInstallUI> ExtensionInstallUI::Create(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<ExtensionInstallUIAndroid>(context);
#else
  return std::make_unique<ExtensionInstallUIDesktop>(context);
#endif
}

ExtensionInstallUI::ExtensionInstallUI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {}

ExtensionInstallUI::~ExtensionInstallUI() = default;

void ExtensionInstallUI::SetUseAppInstalledBubble(bool use_bubble) {
  use_app_installed_bubble_ = use_bubble;
}

void ExtensionInstallUI::SetSkipPostInstallUI(bool skip_ui) {
  skip_post_install_ui_ = skip_ui;
}

// static
base::AutoReset<bool> ExtensionInstallUI::disable_ui_for_tests(bool disable) {
  return base::AutoReset<bool>(&g_disable_ui_for_tests, disable);
}

// static
bool ExtensionInstallUI::IsUiDisabled() {
  return g_disable_ui_for_tests;
}
