// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"
#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kVerticalPadding = 16;

// Compares two UserNoteInstances by their rect's origin, which represents their
// position in a web page. If the UserNoteInstances have the same position,
// compare them by their modification date.
bool UserNoteComparator(const user_notes::UserNoteInstance* first,
                        user_notes::UserNoteInstance* second) {
  if (first->rect() == second->rect()) {
    return first->model().metadata().modification_date() <
           second->model().metadata().modification_date();
  }
  return first->rect() < second->rect();
}

SidePanelCoordinator* GetSidePanelCoordinator(BrowserView* browser_view) {
  if (!browser_view)
    return nullptr;

  return browser_view->side_panel_coordinator();
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(UserNoteUICoordinator,
                                      kScrollViewElementIdForTesting);

// static
void UserNoteUICoordinator::CreateForBrowser(Browser* browser) {
  DCHECK(browser);
  if (!FromBrowser(browser)) {
    browser->SetUserData(user_notes::UserNotesUI::UserDataKey(),
                         base::WrapUnique(new UserNoteUICoordinator(browser)));
  }
}

// static
UserNoteUICoordinator* UserNoteUICoordinator::FromBrowser(Browser* browser) {
  DCHECK(browser);
  return static_cast<UserNoteUICoordinator*>(
      browser->GetUserData(user_notes::UserNotesUI::UserDataKey()));
}

// static
UserNoteUICoordinator* UserNoteUICoordinator::GetOrCreateForBrowser(
    Browser* browser) {
  if (auto* data = FromBrowser(browser)) {
    return data;
  }

  CreateForBrowser(browser);
  return FromBrowser(browser);
}

UserNoteUICoordinator::UserNoteUICoordinator(Browser* browser)
    : browser_(browser) {
  browser_->tab_strip_model()->AddObserver(this);
  is_tab_strip_model_observed_ = true;
}

UserNoteUICoordinator::~UserNoteUICoordinator() = default;

void UserNoteUICoordinator::OnSidePanelDidClose() {
  scroll_view_ = nullptr;
  if (auto* side_panel_coordinator = GetSidePanelCoordinator(browser_view_)) {
    side_panel_coordinator->RemoveSidePanelViewStateObserver(this);
  }
}

void UserNoteUICoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kUserNote,
      l10n_util::GetStringUTF16(IDS_USER_NOTE_TITLE),
      ui::ImageModel::FromVectorIcon(kInkHighlighterIcon, ui::kColorIcon),
      base::BindRepeating(&UserNoteUICoordinator::CreateUserNotesView,
                          base::Unretained(this)));
  entry->AddObserver(this);
  global_registry->Register(std::move(entry));
}

void UserNoteUICoordinator::OnEntryShown(SidePanelEntry* entry) {
  // Do not observe again if tab strip model is already being observed.
  if (is_tab_strip_model_observed_)
    return;

  browser_->tab_strip_model()->AddObserver(this);
  is_tab_strip_model_observed_ = true;
}

void UserNoteUICoordinator::OnEntryHidden(SidePanelEntry* entry) {
  browser_->tab_strip_model()->RemoveObserver(this);
  is_tab_strip_model_observed_ = false;
}

void UserNoteUICoordinator::OnNoteDeleted(const base::UnguessableToken& id,
                                          UserNoteView* user_note_view) {
  scroll_view_->contents()->RemoveChildView(user_note_view);
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteDeleted(id);
  scroll_view_->Layout();
}

void UserNoteUICoordinator::OnNoteSelected(const base::UnguessableToken& id) {
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  // TODO(crbug.com/1313967): This only works because notes are only supported
  // in the primary main frame for now. If notes are ever supported in
  // subframes, this will need to change.
  service->OnNoteSelected(id, browser_->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetPrimaryMainFrame());
}

void UserNoteUICoordinator::OnNoteCreationDone(
    const base::UnguessableToken& id,
    const std::u16string& note_content) {
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteCreationDone(id, note_content);
}

void UserNoteUICoordinator::OnNoteCreationCancelled(
    const base::UnguessableToken& id,
    UserNoteView* user_note_view) {
  scroll_view_->contents()->RemoveChildView(user_note_view);
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteCreationCancelled(id);
}

void UserNoteUICoordinator::OnNoteUpdated(const base::UnguessableToken& id,
                                          const std::u16string& note_content) {
  auto* service =
      user_notes::UserNoteServiceFactory::GetForContext(browser_->profile());
  service->OnNoteEdited(id, note_content);
}

void UserNoteUICoordinator::FocusNote(const base::UnguessableToken& guid) {
  Show();

  auto* scroll_contents_view = scroll_view_->contents();
  for (views::View* child_view : scroll_contents_view->children()) {
    UserNoteView* user_note_view = views::AsViewClass<UserNoteView>(child_view);
    if (user_note_view->user_note_id() == guid) {
      scroll_to_note_id_ = user_note_view->user_note_id();
      ScrollToNote();
      return;
    }
  }
}

void UserNoteUICoordinator::StartNoteCreation(
    user_notes::UserNoteInstance* instance) {
  Show();

  auto* scroll_contents_view = scroll_view_->contents();
  scoped_view_observer_.Observe(scroll_contents_view);
  scroll_to_note_id_ = instance->model().id();

  int index = 0;
  for (views::View* child_view : scroll_contents_view->children()) {
    UserNoteView* user_note_view = views::AsViewClass<UserNoteView>(child_view);
    if (user_note_view->user_note_rect() < instance->rect()) {
      index++;
      continue;
    }
    break;
  }

  scroll_contents_view->AddChildViewAt(
      std::make_unique<UserNoteView>(this, instance,
                                     UserNoteView::State::kCreating),
      index);

  scroll_view_->Layout();
}

void UserNoteUICoordinator::OnViewBoundsChanged(views::View* observed_view) {
  // Scrolling to note can only be done after the view is drawn
  // (bounds has changed), otherwise we cannot get the bounds of each view.
  // After the view is drawn, we don't need to observe it anymore.
  scoped_view_observer_.Reset();
  ScrollToNote();
}

void UserNoteUICoordinator::ScrollToNote() {
  if (scroll_to_note_id_ == base::UnguessableToken::Null())
    return;

  for (views::View* child_content_view : scroll_view_->contents()->children()) {
    UserNoteView* user_note_view =
        views::AsViewClass<UserNoteView>(child_content_view);
    if (user_note_view->user_note_id() == scroll_to_note_id_) {
      child_content_view->ScrollViewToVisible();
      break;
    }
  }

  scroll_to_note_id_ = base::UnguessableToken::Null();
}

void UserNoteUICoordinator::InvalidateIfVisible() {
  if (!browser_view_)
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser_);

  auto* side_panel_coordinator = GetSidePanelCoordinator(browser_view_);

  if (!side_panel_coordinator || side_panel_coordinator->GetCurrentEntryId() !=
                                     SidePanelEntry::Id::kUserNote) {
    return;
  }

  Invalidate();
}

void UserNoteUICoordinator::Invalidate() {
  if (!scroll_view_)
    return;

  if (!browser_->tab_strip_model()->GetActiveWebContents()) {
    if (scroll_view_->contents() &&
        scroll_view_->contents()->children().size() > 0) {
      scroll_view_->contents()->RemoveAllChildViews();
    }
    return;
  }

  auto* user_notes_manager = user_notes::UserNoteManager::GetForPage(
      browser_->tab_strip_model()->GetActiveWebContents()->GetPrimaryPage());

  std::vector<user_notes::UserNoteInstance*> user_note_instances =
      user_notes_manager ? user_notes_manager->GetAllNoteInstances()
                         : std::vector<user_notes::UserNoteInstance*>();
  std::sort(user_note_instances.begin(), user_note_instances.end(),
            UserNoteComparator);

  uint32_t instances_index = 0;
  uint32_t views_index = 0;
  auto* scroll_contents_view = scroll_view_->contents();

  while (instances_index < user_note_instances.size() ||
         views_index < scroll_contents_view->children().size()) {
    // If we've reached the end of the UserNoteInstance vector but not the end
    // of the scroll_contents_view children's vector, we should remove the
    // remaining child views from the scroll_contents_view.
    if (instances_index >= user_note_instances.size()) {
      views::View* user_note_view =
          scroll_contents_view->children().at(views_index);
      scroll_contents_view->RemoveChildView(user_note_view);
      continue;
    }

    user_notes::UserNoteInstance* const user_note_instance =
        user_note_instances.at(instances_index);
    DCHECK(user_note_instance);

    // If we've reached the end of the scroll_contents_view child's vector but
    // not the end of the UserNoteInstance vector, we should create new
    // UserNoteViews from the remaining notes in the UserNoteInstance
    // vector.
    if (views_index >= scroll_contents_view->children().size()) {
      scroll_contents_view->AddChildViewAt(
          std::make_unique<UserNoteView>(this, user_note_instance,
                                         UserNoteView::State::kDefault),
          views_index);
      instances_index++;
      views_index++;
      continue;
    }

    UserNoteView* user_note_view = views::AsViewClass<UserNoteView>(
        scroll_contents_view->children().at(views_index));

    if (user_note_view->user_note_id() == base::UnguessableToken::Null()) {
      // Remove the current UserNoteView from scroll_contents_view if its Id is
      // null.
      scroll_contents_view->RemoveChildView(user_note_view);
      continue;
    }

    if (user_note_view->user_note_id() == user_note_instance->model().id()) {
      instances_index++;
      views_index++;
    } else if (user_note_view->user_note_rect() < user_note_instance->rect()) {
      // Remove the current UserNoteView because the note is no longer available
      // in the UserNoteInstance vector.
      scroll_contents_view->RemoveChildView(user_note_view);
    } else {
      // Add a new UserNoteView because the current UserNoteInstance note is
      // missing from scroll_contents_view's children.
      scroll_contents_view->AddChildViewAt(
          std::make_unique<UserNoteView>(this, user_note_instance,
                                         UserNoteView::State::kDefault),
          views_index);
      instances_index++;
      views_index++;
    }
  }
}

void UserNoteUICoordinator::Show() {
  if (!browser_view_)
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser_);

  auto* side_panel_coordinator = GetSidePanelCoordinator(browser_view_);

  if (!side_panel_coordinator) {
    return;
  }

  if (side_panel_coordinator->GetCurrentEntryId() ==
      SidePanelEntry::Id::kUserNote) {
    return;
  }

  side_panel_coordinator->Show(
      SidePanelEntry::Id::kUserNote,
      SidePanelUtil::SidePanelOpenTrigger::kNotesInPageContextMenu);
}

void UserNoteUICoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed() || tab_strip_model->closing_all())
    return;

  InvalidateIfVisible();
}

std::unique_ptr<views::View> UserNoteUICoordinator::CreateUserNotesView() {
  // Layout structure:
  //
  // [| [NoteView]              | <--- scroll content view ] <--- scroll view
  // [| ...                     |]
  // [| ...                     |]

  if (!browser_view_)
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser_);

  if (auto* side_panel_coordinator = GetSidePanelCoordinator(browser_view_)) {
    side_panel_coordinator->AddSidePanelViewStateObserver(this);
  }

  auto root_view = std::make_unique<views::View>();
  root_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  scroll_view_ = root_view->AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  scroll_view_->SetProperty(views::kElementIdentifierKey,
                            kScrollViewElementIdForTesting);
  // Setting clip height is necessary to make ScrollView take into account its
  // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
  // correctly.
  scroll_view_->ClipHeightTo(0, 0);

  // TODO(cheickcisse): Populate scroll content view.
  views::View* scroll_contents_view =
      scroll_view_->SetContents(std::make_unique<views::View>());

  constexpr int edge_margin = 16;
  auto* layout =
      scroll_contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(kVerticalPadding, edge_margin), kVerticalPadding));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  Invalidate();
  return root_view;
}

std::unique_ptr<views::View> UserNoteUICoordinator::CreateUserNotesWebUIView() {
  auto view = std::make_unique<SidePanelWebUIViewT<UserNotesSidePanelUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<BubbleContentsWrapperT<UserNotesSidePanelUI>>(
          GURL(chrome::kChromeUIUserNotesSidePanelURL), browser_->profile(),
          IDS_USER_NOTE_TITLE,
          /*webui_resizes_host=*/false,
          /*esc_closes_ui=*/false));
  // TODO(corising): Remove this and appropriately update availability based on
  // notes ui readiness.
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}
