// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class ExtensionInstalledBubbleViewsBrowserTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ExtensionInstalledBubbleViewsBrowserTest() = default;
  ~ExtensionInstalledBubbleViewsBrowserTest() override = default;

  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 private:
  scoped_refptr<const extensions::Extension> MakeExtensionOfType(
      const std::string& type) {
    extensions::ExtensionBuilder builder(type);

    if (type == "BrowserAction") {
      builder.SetAction(extensions::ActionInfo::Type::kBrowser);
    } else if (type == "PageAction") {
      builder.SetAction(extensions::ActionInfo::Type::kPage);
    }

    if (type == "SignInPromo" || type == "NoAction") {
      builder.SetLocation(extensions::mojom::ManifestLocation::kInternal);
    } else {
      builder.SetLocation(extensions::mojom::ManifestLocation::kComponent);
    }

    if (type == "Omnibox") {
      base::Value::Dict extra_keys;
      extra_keys.SetByDottedPath(extensions::manifest_keys::kOmniboxKeyword,
                                 "foo");
      builder.MergeManifest(std::move(extra_keys));
    }

    scoped_refptr<const extensions::Extension> extension = builder.Build();
    extension_service()->AddExtension(extension.get());
    return extension;
  }

  raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> bubble_widget_;
};

void ExtensionInstalledBubbleViewsBrowserTest::ShowUi(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension =
      MakeExtensionOfType(name);

  views::Widget::Widgets old_widgets = views::test::WidgetTest::GetAllWidgets();
  ExtensionInstallUI::ShowBubble(extension, browser(), SkBitmap());
  views::Widget::Widgets new_widgets = views::test::WidgetTest::GetAllWidgets();
  views::Widget::Widgets added_widgets;
  std::set_difference(new_widgets.begin(), new_widgets.end(),
                      old_widgets.begin(), old_widgets.end(),
                      std::inserter(added_widgets, added_widgets.begin()));
  ASSERT_EQ(added_widgets.size(), 1u);
  bubble_widget_ = *added_widgets.begin();

  // The extension slides out of the extensions menu before the bubble shows.
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget_).Wait();
}

bool ExtensionInstalledBubbleViewsBrowserTest::VerifyUi() {
  return bubble_widget_->IsVisible();
}

void ExtensionInstalledBubbleViewsBrowserTest::WaitForUserDismissal() {
  views::test::WidgetDestroyedWaiter observer(bubble_widget_);
  observer.Wait();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// None of these tests work when run under Ash, because they need an
// AuraTestHelper constructed at an inconvenient time in test setup, which
// InProcessBrowserTest is not equipped to handle.
// TODO(ellyjones): Fix that, or figure out an alternate way to test this UI.
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#define MAYBE_InvokeUi_BrowserAction DISABLED_InvokeUi_BrowserAction
#define MAYBE_InvokeUi_PageAction DISABLED_InvokeUi_PageAction
#define MAYBE_InvokeUi_SignInPromo DISABLED_InvokeUi_SignInPromo
#define MAYBE_InvokeUi_Omnibox DISABLED_InvokeUi_Omnibox
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#define MAYBE_InvokeUi_BrowserAction InvokeUi_BrowserAction
#define MAYBE_InvokeUi_PageAction InvokeUi_PageAction
#define MAYBE_InvokeUi_SignInPromo InvokeUi_SignInPromo
#define MAYBE_InvokeUi_Omnibox InvokeUi_Omnibox
#endif

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_BrowserAction) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_PageAction) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_SignInPromo) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsBrowserTest,
                       MAYBE_InvokeUi_Omnibox) {
  ShowAndVerifyUi();
}
