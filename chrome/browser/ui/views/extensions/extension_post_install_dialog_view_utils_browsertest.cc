// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/extensions/extension_post_install_dialog.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class ExtensionPostInstallDialogViewUtilsBrowserTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ExtensionPostInstallDialogViewUtilsBrowserTest() = default;
  ~ExtensionPostInstallDialogViewUtilsBrowserTest() override = default;

  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 private:
  scoped_refptr<const extensions::Extension> MakeExtensionOfType(
      const std::string& type) {
    extensions::ExtensionBuilder builder(type);

    if (type == "SignInPromo" || type == "NoAction") {
      builder.SetLocation(extensions::mojom::ManifestLocation::kInternal);
    } else {
      builder.SetLocation(extensions::mojom::ManifestLocation::kComponent);
    }

    return builder.Build();
  }

  raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> bubble_widget_;
};

void ExtensionPostInstallDialogViewUtilsBrowserTest::ShowUi(
    const std::string& name) {
  scoped_refptr<const extensions::Extension> extension =
      MakeExtensionOfType(name);

  views::Widget::Widgets old_widgets = views::test::WidgetTest::GetAllWidgets();
  extensions::TriggerPostInstallDialog(
      profile(), extension, SkBitmap(),
      base::BindOnce(
          [](Browser* b) {
            return b->tab_strip_model()->GetActiveWebContents();
          },
          browser()));

  // In the real world, the extension would be installed now, triggering the
  // watcher. Simulate that by adding the extension to the registrar.
  extension_registrar()->AddExtension(extension);

  // Wait for the ExtensionInstalledWatcher to fire and the dialog to be
  // created.
  (void)base::test::RunUntil([&]() {
    return views::test::WidgetTest::GetAllWidgets().size() > old_widgets.size();
  });

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

bool ExtensionPostInstallDialogViewUtilsBrowserTest::VerifyUi() {
  return bubble_widget_->IsVisible();
}

void ExtensionPostInstallDialogViewUtilsBrowserTest::WaitForUserDismissal() {
  views::test::WidgetDestroyedWaiter observer(bubble_widget_);
  observer.Wait();
}

#if BUILDFLAG(IS_CHROMEOS)
// None of these tests work when run under Ash, because they need an
// AuraTestHelper constructed at an inconvenient time in test setup, which
// InProcessBrowserTest is not equipped to handle.
// TODO(ellyjones): Fix that, or figure out an alternate way to test this UI.
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#define MAYBE_InvokeUi_SignInPromo DISABLED_InvokeUi_SignInPromo
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#define MAYBE_InvokeUi_SignInPromo InvokeUi_SignInPromo
#endif

IN_PROC_BROWSER_TEST_F(ExtensionPostInstallDialogViewUtilsBrowserTest,
                       MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionPostInstallDialogViewUtilsBrowserTest,
                       MAYBE_InvokeUi_SignInPromo) {
  ShowAndVerifyUi();
}
