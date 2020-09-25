// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/md_text_button_with_down_arrow.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_sink_button.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/media_sink.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace media_router {

namespace {

// This value is negative so that it doesn't overlap with a sink index.
constexpr int kAlternativeSourceButtonId = -1;

std::unique_ptr<views::Button> CreateSourcesButton(
    views::ButtonListener* listener) {
  auto sources_button = std::make_unique<views::MdTextButtonWithDownArrow>(
      listener,
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ALTERNATIVE_SOURCES_BUTTON));
  sources_button->SetID(kAlternativeSourceButtonId);
  sources_button->SetEnabled(false);
  return sources_button;
}

}  // namespace

// static
void CastDialogView::ShowDialogWithToolbarAction(
    CastDialogController* controller,
    Browser* browser,
    const base::Time& start_time,
    MediaRouterDialogOpenOrigin activation_location) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* action_view = browser_view->toolbar()->cast_button();
  DCHECK(action_view);
  ShowDialog(action_view, views::BubbleBorder::TOP_RIGHT, controller,
             browser->profile(), start_time, activation_location);
}

// static
void CastDialogView::ShowDialogCenteredForBrowserWindow(
    CastDialogController* controller,
    Browser* browser,
    const base::Time& start_time,
    MediaRouterDialogOpenOrigin activation_location) {
  ShowDialog(BrowserView::GetBrowserViewForBrowser(browser)->top_container(),
             views::BubbleBorder::TOP_CENTER, controller, browser->profile(),
             start_time, activation_location);
}

// static
void CastDialogView::ShowDialogCentered(
    const gfx::Rect& bounds,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogOpenOrigin activation_location) {
  ShowDialog(/* anchor_view */ nullptr, views::BubbleBorder::TOP_CENTER,
             controller, profile, start_time, activation_location);
  instance_->SetAnchorRect(bounds);
}

// static
void CastDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();
  // We set |instance_| to null here because IsShowing() should be false after
  // HideDialog() is called. Not all paths to close the dialog go through
  // HideDialog(), so we also set it to null in WindowClosing(), which always
  // gets called asynchronously.
  instance_ = nullptr;
}

// static
bool CastDialogView::IsShowing() {
  return instance_ != nullptr;
}

// static
CastDialogView* CastDialogView::GetInstance() {
  return instance_;
}

// static
views::Widget* CastDialogView::GetCurrentDialogWidget() {
  return instance_ ? instance_->GetWidget() : nullptr;
}

base::string16 CastDialogView::GetWindowTitle() const {
  switch (selected_source_) {
    case SourceType::kTab:
      return dialog_title_;
    case SourceType::kDesktop:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
    case SourceType::kLocalFile:
      return l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_CAST_LOCAL_MEDIA_TITLE,
                                        local_file_name_.value());
    default:
      NOTREACHED();
      return base::string16();
  }
}

void CastDialogView::OnModelUpdated(const CastDialogModel& model) {
  if (model.media_sinks().empty()) {
    scroll_position_ = 0;
    ShowNoSinksView();
    if (sources_button_)
      sources_button_->SetEnabled(false);
  } else {
    if (scroll_view_)
      scroll_position_ = scroll_view_->GetVisibleRect().y();
    else
      ShowScrollView();
    PopulateScrollView(model.media_sinks());
    RestoreSinkListState();
    metrics_.OnSinksLoaded(base::Time::Now());
    if (sources_button_)
      sources_button_->SetEnabled(true);
    DisableUnsupportedSinks();
  }
  dialog_title_ = model.dialog_header();
  MaybeSizeToContents();
  // Update the main action button.
  DialogModelChanged();
  for (Observer& observer : observers_)
    observer.OnDialogModelUpdated(this);
}

void CastDialogView::OnControllerInvalidated() {
  controller_ = nullptr;
  // We don't call HideDialog() here because if the invalidation was caused by
  // activating the toolbar icon in order to close the dialog, then it would
  // cause the dialog to immediately open again.
}

void CastDialogView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender->tag() == kAlternativeSourceButtonId) {
    ShowSourcesMenu();
  } else {
    // SinkPressed() invokes a refresh of the sink list, which deletes the
    // sink button. So we must call this after the button is done handling the
    // press event.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CastDialogView::SinkPressed,
                                  weak_factory_.GetWeakPtr(), sender->tag()));
  }
}

gfx::Size CastDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

void CastDialogView::OnPaint(gfx::Canvas* canvas) {
  views::BubbleDialogDelegateView::OnPaint(canvas);
  metrics_.OnPaint(base::Time::Now());
}

bool CastDialogView::IsCommandIdChecked(int command_id) const {
  return command_id == selected_source_;
}

bool CastDialogView::IsCommandIdEnabled(int command_id) const {
  return true;
}

void CastDialogView::ExecuteCommand(int command_id, int event_flags) {
  // This method is called when the user selects a source in the source picker.
  if (command_id == SourceType::kLocalFile) {
    // When the file picker dialog opens, the Cast dialog loses focus. So we
    // must temporarily prevent it from closing when losing focus.
    set_close_on_deactivate(false);
    controller_->ChooseLocalFile(base::BindOnce(
        &CastDialogView::OnFilePickerClosed, weak_factory_.GetWeakPtr()));
  } else {
    if (local_file_name_)
      local_file_name_.reset();
    SelectSource(static_cast<SourceType>(command_id));
  }
}

void CastDialogView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastDialogView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CastDialogView::KeepShownForTesting() {
  keep_shown_for_testing_ = true;
  set_close_on_deactivate(false);
}

// static
void CastDialogView::ShowDialog(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogOpenOrigin activation_location) {
  DCHECK(!instance_);
  DCHECK(!start_time.is_null());
  instance_ = new CastDialogView(anchor_view, anchor_position, controller,
                                 profile, start_time, activation_location);
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

CastDialogView::CastDialogView(views::View* anchor_view,
                               views::BubbleBorder::Arrow anchor_position,
                               CastDialogController* controller,
                               Profile* profile,
                               const base::Time& start_time,
                               MediaRouterDialogOpenOrigin activation_location)
    : BubbleDialogDelegateView(anchor_view, anchor_position),
      selected_source_(SourceType::kTab),
      controller_(controller),
      profile_(profile),
      metrics_(start_time, activation_location, profile) {
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  sources_button_ = SetExtraView(CreateSourcesButton(this));
  ShowNoSinksView();
}

CastDialogView::~CastDialogView() {
  if (controller_)
    controller_->RemoveObserver(this);
}

void CastDialogView::Init() {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  controller_->AddObserver(this);
  RecordSinkCountWithDelay();
}

void CastDialogView::WindowClosing() {
  for (Observer& observer : observers_)
    observer.OnDialogWillClose(this);
  if (instance_ == this)
    instance_ = nullptr;
  metrics_.OnCloseDialog(base::Time::Now());
}

void CastDialogView::ShowNoSinksView() {
  if (no_sinks_view_)
    return;
  if (scroll_view_) {
    // The dtor of |scroll_view_| removes it from the dialog.
    delete scroll_view_;
    scroll_view_ = nullptr;
    sink_buttons_.clear();
  }
  no_sinks_view_ = new CastDialogNoSinksView(profile_);
  AddChildView(no_sinks_view_);
}

void CastDialogView::ShowScrollView() {
  if (scroll_view_)
    return;
  if (no_sinks_view_) {
    // The dtor of |no_sinks_view_| removes it from the dialog.
    delete no_sinks_view_;
    no_sinks_view_ = nullptr;
  }
  scroll_view_ = new views::ScrollView();
  AddChildView(scroll_view_);
  constexpr int kSinkButtonHeight = 56;
  scroll_view_->ClipHeightTo(0, kSinkButtonHeight * 6.5);
}

void CastDialogView::RestoreSinkListState() {
  if (selected_sink_index_ &&
      selected_sink_index_.value() < sink_buttons_.size()) {
    CastDialogSinkButton* sink_button =
        sink_buttons_.at(selected_sink_index_.value());
    // Focus on the sink so that the screen reader reads its label, which has
    // likely been updated.
    sink_button->RequestFocus();
    // If the state became AVAILABLE, the screen reader no longer needs to read
    // the label until the user selects a sink again.
    if (sink_button->sink().state == UIMediaSinkState::AVAILABLE)
      selected_sink_index_.reset();
  }

  views::ScrollBar* scroll_bar =
      const_cast<views::ScrollBar*>(scroll_view_->vertical_scroll_bar());
  if (scroll_bar) {
    scroll_view_->ScrollToPosition(scroll_bar, scroll_position_);
    scroll_view_->Layout();
  }
}

void CastDialogView::PopulateScrollView(const std::vector<UIMediaSink>& sinks) {
  sink_buttons_.clear();
  auto sink_list_view = std::make_unique<views::View>();
  sink_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (size_t i = 0; i < sinks.size(); i++) {
    const UIMediaSink& sink = sinks.at(i);
    CastDialogSinkButton* sink_button =
        new CastDialogSinkButton(this, sink, /** button_tag */ i);
    sink_buttons_.push_back(sink_button);
    sink_list_view->AddChildView(sink_button);
  }
  scroll_view_->SetContents(std::move(sink_list_view));

  MaybeSizeToContents();
  Layout();
}

void CastDialogView::ShowSourcesMenu() {
  if (!sources_menu_model_) {
    sources_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);

    sources_menu_model_->AddCheckItemWithStringId(
        SourceType::kTab, IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE);
    sources_menu_model_->AddCheckItemWithStringId(
        SourceType::kDesktop, IDS_MEDIA_ROUTER_DESKTOP_MIRROR_CAST_MODE);
    sources_menu_model_->AddCheckItemWithStringId(
        SourceType::kLocalFile, IDS_MEDIA_ROUTER_LOCAL_FILE_CAST_MODE);
  }

  sources_menu_runner_ = std::make_unique<views::MenuRunner>(
      sources_menu_model_.get(), views::MenuRunner::COMBOBOX);
  const gfx::Rect& screen_bounds = sources_button_->GetBoundsInScreen();
  sources_menu_runner_->RunMenuAt(
      sources_button_->GetWidget(), nullptr, screen_bounds,
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
}

void CastDialogView::SelectSource(SourceType source) {
  selected_source_ = source;
  DisableUnsupportedSinks();
  GetWidget()->UpdateWindowTitle();
  metrics_.OnCastModeSelected();
}

void CastDialogView::SinkPressed(size_t index) {
  if (!controller_)
    return;

  selected_sink_index_ = index;
  const UIMediaSink& sink = sink_buttons_.at(index)->sink();
  if (sink.route) {
    metrics_.OnStopCasting(sink.route->is_local());
    // StopCasting() may trigger a model update and invalidate |sink|.
    controller_->StopCasting(sink.route->media_route_id());
  } else if (sink.issue) {
    controller_->ClearIssue(sink.issue->id());
  } else {
    // |sink| may get invalidated during CastDialogController::StartCasting()
    // due to a model update, so we must use a copy of its |id| field.
    const std::string sink_id = sink.id;
    base::Optional<MediaCastMode> cast_mode = GetCastModeToUse(sink);
    if (cast_mode) {
      // Starting local file casting may open a new tab synchronously on the UI
      // thread, which deactivates the dialog. So we must prevent it from
      // closing and getting destroyed.
      if (cast_mode.value() == LOCAL_FILE)
        set_close_on_deactivate(false);
      controller_->StartCasting(sink_id, cast_mode.value());
      // Re-enable close on deactivate so the user can click elsewhere to close
      // the dialog.
      if (cast_mode.value() == LOCAL_FILE)
        set_close_on_deactivate(!keep_shown_for_testing_);
      metrics_.OnStartCasting(base::Time::Now(), index, cast_mode.value());
    }
  }
}

void CastDialogView::MaybeSizeToContents() {
  // The widget may be null if this is called while the dialog is opening.
  if (GetWidget())
    SizeToContents();
}

base::Optional<MediaCastMode> CastDialogView::GetCastModeToUse(
    const UIMediaSink& sink) const {
  // Go through cast modes in the order of preference to find one that is
  // supported and selected.
  switch (selected_source_) {
    case SourceType::kTab:
      if (base::Contains(sink.cast_modes, PRESENTATION))
        return base::make_optional<MediaCastMode>(PRESENTATION);
      if (base::Contains(sink.cast_modes, TAB_MIRROR))
        return base::make_optional<MediaCastMode>(TAB_MIRROR);
      break;
    case SourceType::kDesktop:
      if (base::Contains(sink.cast_modes, DESKTOP_MIRROR))
        return base::make_optional<MediaCastMode>(DESKTOP_MIRROR);
      break;
    case SourceType::kLocalFile:
      if (base::Contains(sink.cast_modes, LOCAL_FILE))
        return base::make_optional<MediaCastMode>(LOCAL_FILE);
      break;
  }
  return base::nullopt;
}

void CastDialogView::DisableUnsupportedSinks() {
  // Go through the AVAILABLE sinks and enable or disable them depending on
  // whether they support the selected cast mode.
  for (CastDialogSinkButton* sink_button : sink_buttons_) {
    if (sink_button->sink().state != UIMediaSinkState::AVAILABLE)
      continue;
    const bool enable = GetCastModeToUse(sink_button->sink()).has_value();
    sink_button->SetEnabled(enable);
  }
}

void CastDialogView::RecordSinkCountWithDelay() {
  // Record the number of sinks after three seconds. This is consistent with the
  // WebUI dialog.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastDialogView::RecordSinkCount,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(3));
}

void CastDialogView::RecordSinkCount() {
  metrics_.OnRecordSinkCount(sink_buttons_.size());
}

void CastDialogView::OnFilePickerClosed(const ui::SelectedFileInfo* file_info) {
  // Re-enable the setting to close the dialog when it loses focus.
  set_close_on_deactivate(!keep_shown_for_testing_);
  if (file_info) {
#if defined(OS_WIN)
    local_file_name_ = file_info->display_name;
#else
    local_file_name_ = base::UTF8ToUTF16(file_info->display_name);
#endif  // defined(OS_WIN)
    SelectSource(SourceType::kLocalFile);
  }
}

// static
CastDialogView* CastDialogView::instance_ = nullptr;

}  // namespace media_router
