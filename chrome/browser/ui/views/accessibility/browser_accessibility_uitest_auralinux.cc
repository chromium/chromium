// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <stddef.h>

#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/views/accessibility/view_accessibility.h"

class AuraLinuxAccessibilityInProcessBrowserTest : public InProcessBrowserTest {
 protected:
  AuraLinuxAccessibilityInProcessBrowserTest() = default;

  void PreRunTestOnMainThread() override {
    ax_mode_override_ =
        std::make_unique<content::ScopedAccessibilityModeOverride>(
            ui::kAXModeComplete);
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void PostRunTestOnMainThread() override {
    InProcessBrowserTest::PostRunTestOnMainThread();
    ax_mode_override_.reset();
  }

  void VerifyEmbedRelationships();

 private:
  std::unique_ptr<content::ScopedAccessibilityModeOverride> ax_mode_override_;
};

IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       IndexInParent) {
  AtkObject* native_view_accessible =
      static_cast<BrowserView*>(browser()->window())->GetNativeViewAccessible();
  EXPECT_NE(nullptr, native_view_accessible);

  int n_children = atk_object_get_n_accessible_children(native_view_accessible);
  for (int i = 0; i < n_children; i++) {
    AtkObject* child =
        atk_object_ref_accessible_child(native_view_accessible, i);

    int index_in_parent = atk_object_get_index_in_parent(child);
    ASSERT_NE(-1, index_in_parent);
    ASSERT_EQ(i, index_in_parent);

    g_object_unref(child);
  }
}

class TestTabModalConfirmDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  explicit TestTabModalConfirmDialogDelegate(content::WebContents* contents)
      : TabModalConfirmDialogDelegate(contents) {}

  TestTabModalConfirmDialogDelegate(const TestTabModalConfirmDialogDelegate&) =
      delete;
  TestTabModalConfirmDialogDelegate& operator=(
      const TestTabModalConfirmDialogDelegate&) = delete;

  std::u16string GetTitle() override { return u"Dialog Title"; }
  std::u16string GetDialogMessage() override { return std::u16string(); }
};

// Open a tab-modal dialog and test IndexInParent with the modal dialog.
IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       IndexInParentWithModal) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AtkObject* native_view_accessible =
      browser_view->GetWidget()->GetRootView()->GetNativeViewAccessible();
  EXPECT_NE(nullptr, native_view_accessible);

  // The root view has a child that is a client role for Chromium. Also, there
  // can be one more hidden child that is `AnnounceTextView`, created by
  // `PdfOcrController` when it announces text via `RootView::AnnounceTextAs()`
  // on Linux.
  int n_children = atk_object_get_n_accessible_children(native_view_accessible);
  ASSERT_GE(n_children, 1);
  AtkObject* client =
      atk_object_ref_accessible_child(native_view_accessible, 0);
  ASSERT_EQ(0, atk_object_get_index_in_parent(client));

  // Opens a tab-modal dialog.
  content::WebContents* contents = browser_view->GetActiveWebContents();
  auto delegate = std::make_unique<TestTabModalConfirmDialogDelegate>(contents);
  TabModalConfirmDialog* dialog =
      TabModalConfirmDialog::Create(std::move(delegate), contents);

  // The root view still has one child that is a dialog role since if it has a
  // modal dialog it hides the rest of the children. However, there can be one
  // more hidden child that is `AnnounceTextView`, which is created by
  // `PdfOcrController` when it announces text via `RootView::AnnounceTextAs()`
  // on Linux.
  n_children = atk_object_get_n_accessible_children(native_view_accessible);
  ASSERT_GE(n_children, 1);
  AtkObject* dialog_node =
      atk_object_ref_accessible_child(native_view_accessible, 0);
  ASSERT_EQ(0, atk_object_get_index_in_parent(dialog_node));

  // The client has an invalid value for the index in parent since it's hidden
  // by the dialog.
  ASSERT_EQ(-1, atk_object_get_index_in_parent(client));

  dialog->CloseDialog();
  // It has a valid value for the client after the dialog is closed.
  ASSERT_EQ(0, atk_object_get_index_in_parent(client));

  g_object_unref(client);
  g_object_unref(dialog_node);
}

static AtkObject* FindParentFrame(AtkObject* object) {
  while (object) {
    if (atk_object_get_role(object) == ATK_ROLE_FRAME)
      return object;
    object = atk_object_get_parent(object);
  }

  return nullptr;
}

void AuraLinuxAccessibilityInProcessBrowserTest::VerifyEmbedRelationships() {
  AtkObject* native_view_accessible =
      static_cast<BrowserView*>(browser()->window())->GetNativeViewAccessible();
  EXPECT_NE(nullptr, native_view_accessible);

  AtkObject* window = FindParentFrame(native_view_accessible);
  EXPECT_NE(nullptr, window);

  AtkRelationSet* relations = atk_object_ref_relation_set(window);
  ASSERT_TRUE(atk_relation_set_contains(relations, ATK_RELATION_EMBEDS));

  AtkRelation* embeds_relation =
      atk_relation_set_get_relation_by_type(relations, ATK_RELATION_EMBEDS);
  EXPECT_NE(nullptr, embeds_relation);

  GPtrArray* targets = atk_relation_get_target(embeds_relation);
  EXPECT_NE(nullptr, targets);
  EXPECT_EQ(1u, targets->len);

  AtkObject* target = static_cast<AtkObject*>(g_ptr_array_index(targets, 0));

  EXPECT_NE(nullptr, target);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(target, active_web_contents->GetRenderWidgetHostView()
                        ->GetNativeViewAccessible());

  g_object_unref(relations);

  relations = atk_object_ref_relation_set(target);
  ASSERT_TRUE(atk_relation_set_contains(relations, ATK_RELATION_EMBEDDED_BY));

  AtkRelation* embedded_by_relation = atk_relation_set_get_relation_by_type(
      relations, ATK_RELATION_EMBEDDED_BY);
  EXPECT_NE(nullptr, embedded_by_relation);

  targets = atk_relation_get_target(embedded_by_relation);
  EXPECT_NE(nullptr, targets);
  EXPECT_EQ(1u, targets->len);

  target = static_cast<AtkObject*>(g_ptr_array_index(targets, 0));
  ASSERT_EQ(target, window);

  g_object_unref(relations);
}

IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       EmbeddedRelationship) {
  // Force the creation of the document's native object which sets up the
  // relationship.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents->GetRenderWidgetHostView()
                         ->GetNativeViewAccessible());

  GURL url(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  VerifyEmbedRelationships();

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  VerifyEmbedRelationships();

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  VerifyEmbedRelationships();
}

// Tests that the embedded relationship is set on the main web contents when
// the DevTools is opened.
// This fails on Linux : http://crbug.com/1223047
#if BUILDFLAG(IS_LINUX)
#define MAYBE_EmbeddedRelationshipWithDevTools \
  DISABLED_EmbeddedRelationshipWithDevTools
#else
#define MAYBE_EmbeddedRelationshipWithDevTools EmbeddedRelationshipWithDevTools
#endif
IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       MAYBE_EmbeddedRelationshipWithDevTools) {
  // Force the creation of the document's native object which sets up the
  // relationship.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents->GetRenderWidgetHostView()
                         ->GetNativeViewAccessible());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Opens DevTools docked.
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  VerifyEmbedRelationships();

  // Closes the DevTools window.
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  VerifyEmbedRelationships();

  // Opens DevTools in a separate window.
  devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  VerifyEmbedRelationships();

  // Closes the DevTools window.
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  VerifyEmbedRelationships();
}

// Tests that it doesn't have DCHECK() error when GetIndexInParent() is called
// with the WebView.
IN_PROC_BROWSER_TEST_F(AuraLinuxAccessibilityInProcessBrowserTest,
                       GetIndexInParent) {
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents->GetRenderWidgetHostView()
                         ->GetNativeViewAccessible());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::WebView* webview = browser_view->contents_web_view();
  gfx::NativeViewAccessible accessible =
      webview->GetViewAccessibility().GetNativeObject();

  // Gets the index in its parents for the WebView.
  std::optional<int> index =
      static_cast<ui::AXPlatformNodeBase*>(
          ui::AXPlatformNode::FromNativeViewAccessible(accessible))
          ->GetIndexInParent();

  // As the WebView is not exposed in the child list when it has the web
  // content, it doesn't have the index in its parent.
  EXPECT_EQ(false, index.has_value());
}
