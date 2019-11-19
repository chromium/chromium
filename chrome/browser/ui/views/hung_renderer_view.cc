// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/hang_monitor/hang_crash_dump.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/hung_renderer/hung_renderer_core.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using content::WebContents;

HungRendererDialogView* HungRendererDialogView::g_instance_ = nullptr;

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, public:

HungPagesTableModel::HungPagesTableModel(Delegate* delegate)
    : delegate_(delegate) {}

HungPagesTableModel::~HungPagesTableModel() {}

content::RenderWidgetHost* HungPagesTableModel::GetRenderWidgetHost() {
  return render_widget_host_;
}

void HungPagesTableModel::InitForWebContents(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  DCHECK(contents);
  DCHECK(render_widget_host);
  DCHECK(!hang_monitor_restarter.is_null());

  DCHECK(!render_widget_host_);
  DCHECK(!process_observer_.IsObservingSources());
  DCHECK(!widget_observer_.IsObservingSources());
  DCHECK(tab_observers_.empty());

  render_widget_host_ = render_widget_host;
  hang_monitor_restarter_ = std::move(hang_monitor_restarter);

  for (auto* hung_contents :
       GetHungWebContentsList(contents, render_widget_host->GetProcess())) {
    tab_observers_.push_back(
        std::make_unique<WebContentsObserverImpl>(this, hung_contents));
  }

  process_observer_.Add(render_widget_host_->GetProcess());
  widget_observer_.Add(render_widget_host_);

  // The world is different.
  if (observer_)
    observer_->OnModelChanged();
}

void HungPagesTableModel::Reset() {
  process_observer_.RemoveAll();
  widget_observer_.RemoveAll();
  tab_observers_.clear();
  render_widget_host_ = nullptr;
}

void HungPagesTableModel::RestartHangMonitorTimeout() {
  if (hang_monitor_restarter_)
    hang_monitor_restarter_.Run();
}

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, ui::TableModel implementation:

int HungPagesTableModel::RowCount() {
  return static_cast<int>(tab_observers_.size());
}

base::string16 HungPagesTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  return GetHungWebContentsTitle(tab_observers_[row]->web_contents(),
                                 render_widget_host_->GetProcess());
}

gfx::ImageSkia HungPagesTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return favicon::ContentFaviconDriver::FromWebContents(
             tab_observers_[row]->web_contents())
      ->GetFavicon()
      .AsImageSkia();
}

void HungPagesTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, RenderProcessHostObserver implementation:

void HungPagesTableModel::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // Notify the delegate.
  delegate_->TabDestroyed();
  // WARNING: we've likely been deleted.
}

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, RenderWidgetHostObserver implementation:

void HungPagesTableModel::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  // Notify the delegate.
  delegate_->TabDestroyed();
  // WARNING: we've likely been deleted.
}

void HungPagesTableModel::TabDestroyed(WebContentsObserverImpl* tab) {
  // Clean up tab_observers_ and notify our observer.
  size_t index = 0;
  for (; index < tab_observers_.size(); ++index) {
    if (tab_observers_[index].get() == tab)
      break;
  }
  DCHECK(index < tab_observers_.size());
  tab_observers_.erase(tab_observers_.begin() + index);
  if (observer_)
    observer_->OnItemsRemoved(static_cast<int>(index), 1);

  // Notify the delegate.
  delegate_->TabDestroyed();
  // WARNING: we've likely been deleted.
}

void HungPagesTableModel::TabUpdated(WebContentsObserverImpl* tab) {
  delegate_->TabUpdated();
}

HungPagesTableModel::WebContentsObserverImpl::WebContentsObserverImpl(
    HungPagesTableModel* model, WebContents* tab)
    : content::WebContentsObserver(tab),
      model_(model) {
}

void HungPagesTableModel::WebContentsObserverImpl::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  // If |new_host| is currently responsive dismiss this dialog, otherwise
  // let the model know the tab has been updated. Updating the tab will
  // dismiss the current dialog but restart the hung renderer timeout.
  if (!new_host->GetWidget()->IsCurrentlyUnresponsive()) {
    model_->TabDestroyed(this);
    return;
  }
  model_->TabUpdated(this);
}

void HungPagesTableModel::WebContentsObserverImpl::WebContentsDestroyed() {
  model_->TabDestroyed(this);
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView

// The dimensions of the hung pages list table view, in pixels.
namespace {
constexpr int kTableViewWidth = 300;
constexpr int kTableViewHeight = 80;
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, public:

// static
HungRendererDialogView* HungRendererDialogView::Create(
    gfx::NativeWindow context) {
  if (!g_instance_) {
    g_instance_ = new HungRendererDialogView;
    views::DialogDelegate::CreateDialogWidget(g_instance_, context, nullptr);
  }
  return g_instance_;
}

// static
HungRendererDialogView* HungRendererDialogView::GetInstance() {
  return g_instance_;
}

// static
void HungRendererDialogView::Show(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  if (logging::DialogsAreSuppressed())
    return;

  gfx::NativeWindow window =
      platform_util::GetTopLevel(contents->GetNativeView());
#if defined(USE_AURA)
  // Don't show the dialog if there is no root window for the renderer, because
  // it's invisible to the user (happens when the renderer is for prerendering
  // for example).
  if (!window->GetRootWindow())
    return;
#endif
  HungRendererDialogView* view = HungRendererDialogView::Create(window);
  view->ShowForWebContents(contents, render_widget_host,
                           std::move(hang_monitor_restarter));
}

// static
void HungRendererDialogView::Hide(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host) {
  if (!logging::DialogsAreSuppressed() && HungRendererDialogView::GetInstance())
    HungRendererDialogView::GetInstance()->EndForWebContents(
        contents, render_widget_host);
}

// static
bool HungRendererDialogView::IsFrameActive(WebContents* contents) {
  gfx::NativeWindow window =
      platform_util::GetTopLevel(contents->GetNativeView());
  return platform_util::IsWindowActive(window);
}

HungRendererDialogView::HungRendererDialogView()
    : info_label_(nullptr), hung_pages_table_(nullptr), initialized_(false) {
#if defined(OS_WIN)
  // Never use the custom frame when Aero Glass is disabled. See
  // https://crbug.com/323278
  DialogDelegate::set_use_custom_frame(ui::win::IsAeroGlassEnabled());
#endif
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::CONTROL));
  chrome::RecordDialogCreation(chrome::DialogIdentifier::HUNG_RENDERER);
}

HungRendererDialogView::~HungRendererDialogView() {
  hung_pages_table_->SetModel(nullptr);
}

void HungRendererDialogView::ShowForWebContents(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  DCHECK(contents && GetWidget());

  // Don't show the warning unless the foreground window is the frame, or this
  // window (but still invisible). If the user has another window or
  // application selected, activating ourselves is rude.
  if (!IsFrameActive(contents) &&
      !platform_util::IsWindowActive(GetWidget()->GetNativeWindow()))
    return;

  if (!GetWidget()->IsActive()) {
    // Place the dialog over content's browser window, similar to modal dialogs.
    Browser* browser = chrome::FindBrowserWithWebContents(contents);
    if (browser) {
      ChromeWebModalDialogManagerDelegate* manager = browser;
      constrained_window::UpdateWidgetModalDialogPosition(
          GetWidget(), manager->GetWebContentsModalDialogHost());
    }

    gfx::NativeWindow window =
        platform_util::GetTopLevel(contents->GetNativeView());
    views::Widget* insert_after =
        views::Widget::GetWidgetForNativeWindow(window);
    if (insert_after)
      GetWidget()->StackAboveWidget(insert_after);

#if defined(OS_WIN)
    // Group the hung renderer dialog with the browsers with the same profile.
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    ui::win::SetAppIdForWindow(
        shell_integration::win::GetChromiumModelIdForProfile(
            profile->GetPath()),
        views::HWNDForWidget(GetWidget()));
#endif

    // We only do this if the window isn't active (i.e. hasn't been shown yet,
    // or is currently shown but deactivated for another WebContents). This is
    // because this window is a singleton, and it's possible another active
    // renderer may hang while this one is showing, and we don't want to reset
    // the list of hung pages for a potentially unrelated renderer while this
    // one is showing.
    hung_pages_table_model_->InitForWebContents(
        contents, render_widget_host, std::move(hang_monitor_restarter));

    UpdateLabels();

    GetWidget()->Show();
  }
}

void HungRendererDialogView::EndForWebContents(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host) {
  DCHECK(contents);
  if (hung_pages_table_model_->RowCount() == 0 ||
      hung_pages_table_model_->GetRenderWidgetHost() == render_widget_host) {
    CloseDialogWithNoAction();
  }
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::DialogDelegate implementation:

base::string16 HungRendererDialogView::GetWindowTitle() const {
  if (!initialized_)
    return base::string16();

  return l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_HANGMONITOR_RENDERER_TITLE,
      hung_pages_table_model_->RowCount());
}

bool HungRendererDialogView::ShouldShowCloseButton() const {
  return false;
}

void HungRendererDialogView::WindowClosing() {
  // We are going to be deleted soon, so make sure our instance is destroyed.
  g_instance_ = nullptr;
}

bool HungRendererDialogView::Cancel() {
  auto* render_widget_host = hung_pages_table_model_->GetRenderWidgetHost();
  bool currently_unresponsive =
      render_widget_host && render_widget_host->IsCurrentlyUnresponsive();
  UMA_HISTOGRAM_BOOLEAN("Stability.RendererUnresponsiveBeforeTermination",
                        currently_unresponsive);

  content::RenderProcessHost* rph =
      hung_pages_table_model_->GetRenderWidgetHost()->GetProcess();
  if (rph) {
#if defined(OS_LINUX)
    // A generic |CrashDumpHungChildProcess()| is not implemented for Linux.
    // Instead we send an explicit IPC to crash on the renderer's IO thread.
    rph->ForceCrash();
#else
    // Try to generate a crash report for the hung process.
    CrashDumpHungChildProcess(rph->GetProcess().Handle());
    rph->Shutdown(content::RESULT_CODE_HUNG);
#endif
  }
  return true;
}

bool HungRendererDialogView::Accept() {
  RestartHangTimer();
  return true;
}

bool HungRendererDialogView::Close() {
  return Accept();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, HungPagesTableModel::Delegate overrides:

void HungRendererDialogView::TabUpdated() {
  RestartHangTimer();
}

void HungRendererDialogView::TabDestroyed() {
  CloseDialogWithNoAction();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::View overrides:

void HungRendererDialogView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (!initialized_ && details.is_add && details.child == this && GetWidget())
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, private:

void HungRendererDialogView::Init() {
  auto info_label = std::make_unique<views::Label>(
      base::string16(), CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY);
  info_label->SetMultiLine(true);
  info_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  hung_pages_table_model_ = std::make_unique<HungPagesTableModel>(this);
  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn());
  auto hung_pages_table = std::make_unique<views::TableView>(
      hung_pages_table_model_.get(), columns, views::ICON_AND_TEXT, true);
  hung_pages_table_ = hung_pages_table.get();

  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetPluralStringFUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_END,
                                       hung_pages_table_model_->RowCount()));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_WAIT));
  DialogModelChanged();

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  constexpr int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  info_label_ = layout->AddView(std::move(info_label));

  layout->AddPaddingRow(
      views::GridLayout::kFixedSize,
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  layout->StartRow(1.0, kColumnSetId);
  layout->AddView(
      views::TableView::CreateScrollViewWithTable(std::move(hung_pages_table)),
      1, 1, views::GridLayout::FILL, views::GridLayout::FILL, kTableViewWidth,
      kTableViewHeight);

  initialized_ = true;
}

void HungRendererDialogView::RestartHangTimer() {
  // Start waiting again for responsiveness.
  hung_pages_table_model_->RestartHangMonitorTimeout();
}

void HungRendererDialogView::UpdateLabels() {
  GetWidget()->UpdateWindowTitle();
  info_label_->SetText(l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_HANGMONITOR_RENDERER, hung_pages_table_model_->RowCount()));
  // Update the "Exit" button.
  DialogModelChanged();
}

void HungRendererDialogView::CloseDialogWithNoAction() {
  // Drop references to the tab immediately because
  // - Close is async and we don't want hanging references, and
  // - While the dialog is active, [X] maps to restarting the hang timer, but
  //   while closing we don't want that action.
  hung_pages_table_model_->Reset();
  GetWidget()->Close();
}
