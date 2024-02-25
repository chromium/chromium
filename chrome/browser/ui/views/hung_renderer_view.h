// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
}

// Provides functionality to display information about a hung renderer.
class HungPagesTableModel : public ui::TableModel,
                            public content::RenderProcessHostObserver,
                            public content::RenderWidgetHostObserver {
 public:
  class Delegate {
   public:
    // Notification when the model is updated (e.g. new location) yet
    // still hung.
    virtual void TabUpdated() = 0;

    // Notification when the model is destroyed.
    virtual void TabDestroyed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit HungPagesTableModel(Delegate* delegate);
  HungPagesTableModel(const HungPagesTableModel&) = delete;
  HungPagesTableModel& operator=(const HungPagesTableModel&) = delete;
  ~HungPagesTableModel() override;

  void InitForWebContents(content::WebContents* hung_contents,
                          content::RenderWidgetHost* render_widget_host,
                          base::RepeatingClosure hang_monitor_restarter);

  // Resets the model to the uninitialized state (e.g. unregisters observers
  // added by InitForWebContents and disassociates this model from any
  // particular WebContents and/or RenderWidgetHost).
  void Reset();

  void RestartHangMonitorTimeout();

  // Returns the hung RenderWidgetHost, or null if there aren't any WebContents.
  content::RenderWidgetHost* GetRenderWidgetHost();

  // Overridden from ui::TableModel:
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column_id) override;
  ui::ImageModel GetIcon(size_t row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // Overridden from RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Overridden from RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

 private:
  friend class HungRendererDialogViewBrowserTest;

  // Used to track a single WebContents. If the WebContents is destroyed
  // TabDestroyed() is invoked on the model.
  class WebContentsObserverImpl : public content::WebContentsObserver {
   public:
    WebContentsObserverImpl(HungPagesTableModel* model,
                            content::WebContents* tab);
    WebContentsObserverImpl(const WebContentsObserverImpl&) = delete;
    WebContentsObserverImpl& operator=(const WebContentsObserverImpl&) = delete;

    favicon::FaviconDriver* favicon_driver() {
      return favicon::ContentFaviconDriver::FromWebContents(web_contents());
    }

    // WebContentsObserver overrides:
    void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                content::RenderFrameHost* new_host) override;
    void WebContentsDestroyed() override;

   private:
    raw_ptr<HungPagesTableModel> model_;
  };

  // Invoked when a WebContents is destroyed. Cleans up |tab_observers_| and
  // notifies the observer and delegate.
  void TabDestroyed(WebContentsObserverImpl* tab);

  // Invoked when a WebContents have been updated. The title or location of
  // the WebContents may have changed.
  void TabUpdated(WebContentsObserverImpl* tab);

  std::vector<std::unique_ptr<WebContentsObserverImpl>> tab_observers_;

  raw_ptr<ui::TableModelObserver, DanglingUntriaged> observer_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;

  raw_ptr<content::RenderWidgetHost> render_widget_host_ = nullptr;

  // Callback that restarts the hang timeout (e.g. if the user wants to wait
  // some more until the renderer process responds).
  base::RepeatingClosure hang_monitor_restarter_;

  base::ScopedObservation<content::RenderProcessHost,
                          content::RenderProcessHostObserver>
      process_observation_{this};

  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      widget_observation_{this};
};

// This class displays a dialog which contains information about a hung
// renderer process.
class HungRendererDialogView : public views::DialogDelegateView,
                               public HungPagesTableModel::Delegate {
  METADATA_HEADER(HungRendererDialogView, views::DialogDelegateView)

 public:
  HungRendererDialogView(const HungRendererDialogView&) = delete;
  HungRendererDialogView& operator=(const HungRendererDialogView&) = delete;

  // Shows or hides the hung renderer dialog for the given WebContents.
  static void Show(content::WebContents* contents,
                   content::RenderWidgetHost* render_widget_host,
                   base::RepeatingClosure hang_monitor_restarter);
  static void Hide(content::WebContents* contents,
                   content::RenderWidgetHost* render_widget_host);

  // Returns true if there is an instance showing for the given WebContents.
  static bool IsShowingForWebContents(content::WebContents* contents);

  views::TableView* table_for_testing() { return hung_pages_table_; }
  HungPagesTableModel* table_model_for_testing() {
    return hung_pages_table_model_.get();
  }

  // views::DialogDelegateView overrides:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // HungPagesTableModel::Delegate overrides:
  void TabUpdated() override;
  void TabDestroyed() override;

 private:
  friend class HungRendererDialogViewBrowserTest;

  explicit HungRendererDialogView(content::WebContents* web_contents);
  ~HungRendererDialogView() override;

  // Creates an instance for the given WebContents and window.
  static HungRendererDialogView* CreateInstance(content::WebContents* contents,
                                                gfx::NativeWindow window);

  // Gets the instance, if any, for the given WebContents, or null if there is
  // none.
  static HungRendererDialogView* GetInstanceForWebContentsForTests(
      content::WebContents* contents);

  // Shows or hides the dialog. Dispatched to by the `Show()` and `Hide()`
  // static methods.
  void ShowDialog(content::RenderWidgetHost* render_widget_host,
                  base::RepeatingClosure hang_monitor_restarter);
  void EndDialog(content::RenderWidgetHost* render_widget_host);

  // Restart the hang timer, giving the page more time.
  void RestartHangTimer();

  // Crashes the hung renderer.
  void ForceCrashHungRenderer();

  // Resets the association with the WebContents.
  //
  // TODO(avi): Calls to this are rather unfortunately scattered throughout the
  // class, but there doesn't seem to be a place that would work for the three
  // ways that the dialog can go away (the two buttons plus the external
  // closing). Both the destructor and `WindowClosing()` functions are too late.
  // Can it be wired in better?
  void ResetWebContentsAssociation();

  // Updates the labels and the button text of the dialog. Normally called only
  // once when the render process first hangs, right before the dialog is shown.
  // It is separated into its own function so that the browsertest's "show UI"
  // functionality is able to fake a multi-page hang and force the UI to refresh
  // as if multiple pages were legitimately hung.
  void UpdateLabels();

  // Causes the dialog to close with no action taken. Called when the page
  // stops hanging by itself, or when the page or render process goes away.
  void CloseDialogWithNoAction();

  // Bypasses the requirement for the browser window to be active. Only used in
  // tests.
  static void BypassActiveBrowserRequirementForTests();

  // The WebContents that this dialog was created for and is associated with.
  const raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_;

  // The label describing the list.
  raw_ptr<views::Label> info_label_ = nullptr;

  // Controls within the dialog box.
  raw_ptr<views::TableView> hung_pages_table_ = nullptr;

  // The model that provides the contents of the table that shows a list of
  // pages affected by the hang.
  std::unique_ptr<HungPagesTableModel> hung_pages_table_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_H_
