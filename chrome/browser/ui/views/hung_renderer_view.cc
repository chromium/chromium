// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using content::WebContents;

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, public:

HungPagesTableModel::HungPagesTableModel(Delegate* delegate)
    : delegate_(delegate) {}

HungPagesTableModel::~HungPagesTableModel() = default;

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
  DCHECK(tab_observers_.empty());

  render_widget_host_ = render_widget_host;
  hang_monitor_restarter_ = std::move(hang_monitor_restarter);

  for (auto* hung_contents :
       GetHungWebContentsList(contents, render_widget_host->GetProcess())) {
    tab_observers_.push_back(
        std::make_unique<WebContentsObserverImpl>(this, hung_contents));
  }

  process_observation_.Observe(render_widget_host_->GetProcess());
  widget_observation_.Observe(render_widget_host_.get());

  // The world is different.
  if (observer_)
    observer_->OnModelChanged();
}

void HungPagesTableModel::Reset() {
  process_observation_.Reset();
  widget_observation_.Reset();
  tab_observers_.clear();
  render_widget_host_ = nullptr;

  // Inform the table model observers that we cleared the model.
  if (observer_)
    observer_->OnModelChanged();
}

void HungPagesTableModel::RestartHangMonitorTimeout() {
  if (hang_monitor_restarter_)
    hang_monitor_restarter_.Run();
}

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, ui::TableModel implementation:

size_t HungPagesTableModel::RowCount() {
  return tab_observers_.size();
}

std::u16string HungPagesTableModel::GetText(size_t row, int column_id) {
  DCHECK(row < RowCount());
  return GetHungWebContentsTitle(tab_observers_[row]->web_contents(),
                                 render_widget_host_->GetProcess());
}

ui::ImageModel HungPagesTableModel::GetIcon(size_t row) {
  DCHECK(row < RowCount());
  return ui::ImageModel::FromImage(
      favicon::ContentFaviconDriver::FromWebContents(
          tab_observers_[row]->web_contents())
          ->GetFavicon());
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
  DCHECK(widget_observation_.IsObservingSource(render_widget_host_.get()));
  widget_observation_.Reset();
  render_widget_host_ = nullptr;

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
    observer_->OnItemsRemoved(index, 1);

  // Notify the delegate.
  delegate_->TabDestroyed();
  // WARNING: we've likely been deleted.
}

void HungPagesTableModel::TabUpdated(WebContentsObserverImpl* tab) {
  delegate_->TabUpdated();
}

HungPagesTableModel::WebContentsObserverImpl::WebContentsObserverImpl(
    HungPagesTableModel* model,
    WebContents* tab)
    : content::WebContentsObserver(tab), model_(model) {}

void HungPagesTableModel::WebContentsObserverImpl::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!new_host->IsInPrimaryMainFrame())
    return;

  // If |new_host| is currently responsive dismiss this dialog, otherwise
  // let the model know the tab has been updated. Updating the tab will
  // dismiss the current dialog but restart the hung renderer timeout.
  if (!new_host->GetRenderWidgetHost()->IsCurrentlyUnresponsive()) {
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

namespace {

constexpr int kDialogHolderUserDataKey = 0;

struct DialogHolder : public base::SupportsUserData::Data {
  explicit DialogHolder(HungRendererDialogView* dialog) : dialog(dialog) {}

  const raw_ptr<HungRendererDialogView> dialog = nullptr;
};

static bool g_bypass_active_browser_requirement = false;

}  // namespace

// static
void HungRendererDialogView::Show(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  if (logging::DialogsAreSuppressed())
    return;

  if (IsShowingForWebContents(contents))
    return;

  // Only show for WebContents in a browser window.
  if (!chrome::FindBrowserWithTab(contents)) {
    return;
  }

  // Don't show the warning unless the foreground window is the frame. If the
  // user has another window or application selected, activating ourselves is
  // rude. Restart the hang monitor so that if that window comes to the
  // foreground, the hang dialog will eventually show for it.
  if (!platform_util::IsWindowActive(
          platform_util::GetTopLevel(contents->GetNativeView())) &&
      !g_bypass_active_browser_requirement) {
    hang_monitor_restarter.Run();
    return;
  }

  HungRendererDialogView* view = CreateInstance(
      contents, platform_util::GetTopLevel(contents->GetNativeView()));
  view->ShowDialog(render_widget_host, std::move(hang_monitor_restarter));
}

// static
void HungRendererDialogView::Hide(
    WebContents* contents,
    content::RenderWidgetHost* render_widget_host) {
  if (logging::DialogsAreSuppressed())
    return;

  DialogHolder* dialog_holder = static_cast<DialogHolder*>(
      contents->GetUserData(&kDialogHolderUserDataKey));
  if (dialog_holder)
    dialog_holder->dialog->EndDialog(render_widget_host);
}

// static
bool HungRendererDialogView::IsShowingForWebContents(WebContents* contents) {
  return contents->GetUserData(&kDialogHolderUserDataKey) != nullptr;
}

HungRendererDialogView::HungRendererDialogView(WebContents* web_contents)
    : web_contents_(web_contents) {
  SetModalType(ui::mojom::ModalType::kChild);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl));
  auto info_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  info_label->SetMultiLine(true);
  info_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  hung_pages_table_model_ = std::make_unique<HungPagesTableModel>(this);
  const std::vector<ui::TableColumn> columns = {ui::TableColumn()};
  auto hung_pages_table =
      std::make_unique<views::TableView>(hung_pages_table_model_.get(), columns,
                                         views::TableType::kIconAndText, true);
  hung_pages_table_ = hung_pages_table.get();

  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_WAIT));

  SetAcceptCallback(base::BindOnce(&HungRendererDialogView::RestartHangTimer,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &HungRendererDialogView::ForceCrashHungRenderer, base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&HungRendererDialogView::RestartHangTimer,
                                  base::Unretained(this)));

  DialogModelChanged();

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  info_label_ = AddChildView(std::move(info_label));

  AddChildView(
      views::TableView::CreateScrollViewWithTable(std::move(hung_pages_table)))
      ->SetPreferredSize(gfx::Size(kTableViewWidth, kTableViewHeight));
}

HungRendererDialogView::~HungRendererDialogView() {
  hung_pages_table_->SetModel(nullptr);
}

// static
HungRendererDialogView* HungRendererDialogView::CreateInstance(
    WebContents* contents,
    gfx::NativeWindow window) {
  HungRendererDialogView* view = new HungRendererDialogView(contents);
  constrained_window::CreateWebModalDialogViews(view, contents);
  contents->SetUserData(&kDialogHolderUserDataKey,
                        std::make_unique<DialogHolder>(view));

  return view;
}

// static
HungRendererDialogView*
HungRendererDialogView::GetInstanceForWebContentsForTests(
    WebContents* contents) {
  DialogHolder* dialog_holder = static_cast<DialogHolder*>(
      contents->GetUserData(&kDialogHolderUserDataKey));
  if (dialog_holder)
    return dialog_holder->dialog;
  return nullptr;
}

void HungRendererDialogView::ShowDialog(
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  DCHECK(GetWidget());

  hung_pages_table_model_->InitForWebContents(
      web_contents_, render_widget_host, std::move(hang_monitor_restarter));

  UpdateLabels();

  constrained_window::ShowModalDialog(GetWidget()->GetNativeWindow(),
                                      web_contents_);
}

void HungRendererDialogView::EndDialog(
    content::RenderWidgetHost* render_widget_host) {
  if (hung_pages_table_model_->RowCount() == 0 ||
      hung_pages_table_model_->GetRenderWidgetHost() == render_widget_host) {
    CloseDialogWithNoAction();
  }
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::DialogDelegate implementation:

std::u16string HungRendererDialogView::GetWindowTitle() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_HANGMONITOR_RENDERER_TITLE,
      hung_pages_table_model_->RowCount());
}

bool HungRendererDialogView::ShouldShowCloseButton() const {
  return false;
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
// HungRendererDialogView, private:

void HungRendererDialogView::RestartHangTimer() {
  // Start waiting again for responsiveness.
  hung_pages_table_model_->RestartHangMonitorTimeout();
  ResetWebContentsAssociation();
}

void HungRendererDialogView::ForceCrashHungRenderer() {
  content::RenderProcessHost* rph =
      hung_pages_table_model_->GetRenderWidgetHost()->GetProcess();
  if (rph) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // A generic |CrashDumpHungChildProcess()| is not implemented for Linux.
    // Instead we send an explicit IPC to crash on the renderer's IO thread.
    rph->ForceCrash();
#else
    // Try to generate a crash report for the hung process.
    CrashDumpHungChildProcess(rph->GetProcess().Handle());
    rph->Shutdown(content::RESULT_CODE_HUNG);
#endif
  }
  ResetWebContentsAssociation();
}

void HungRendererDialogView::ResetWebContentsAssociation() {
  web_contents_->RemoveUserData(&kDialogHolderUserDataKey);
}

void HungRendererDialogView::UpdateLabels() {
  GetWidget()->UpdateWindowTitle();
  info_label_->SetText(l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_HANGMONITOR_RENDERER, hung_pages_table_model_->RowCount()));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetPluralStringFUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_END,
                                       hung_pages_table_model_->RowCount()));
}

void HungRendererDialogView::CloseDialogWithNoAction() {
  // Drop references to the tab immediately because
  // - Close is async and we don't want hanging references, and
  // - While the dialog is active, [X] maps to restarting the hang timer, but
  //   while closing we don't want that action.
  hung_pages_table_model_->Reset();
  ResetWebContentsAssociation();
  GetWidget()->Close();
}

// static
void HungRendererDialogView::BypassActiveBrowserRequirementForTests() {
  g_bypass_active_browser_requirement = true;
}

BEGIN_METADATA(HungRendererDialogView)
END_METADATA
