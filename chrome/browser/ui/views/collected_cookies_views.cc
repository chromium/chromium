// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/collected_cookies_views.h"

#include <map>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/cookie_info_view.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews,
                                      kTabbedPaneElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews,
                                      kBlockedCookiesTreeElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews,
                                      kAllowedCookiesTreeElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews, kBlockButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews, kAllowButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews, kRemoveButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CollectedCookiesViews,
                                      kClearOnExitButtonId);
namespace {

// Dimensions of the tree views.
constexpr int kTreeViewWidth = 400;
constexpr int kTreeViewHeight = 125;

// Baseline height of the cookie info view. We limit the height of the scroll
// pane for the cookie info so that the overall dialog is not too tall to fit in
// a smaller browser window.
constexpr int kInfoViewHeight = 130;

// Returns a view to hold two buttons with padding between.
std::unique_ptr<views::View> CreateNewButtonView() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto view = std::make_unique<views::View>();
  view->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStretch,
                  views::LayoutAlignment::kCenter,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_BUTTON_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .LinkColumnSizes({0, 2})
      .SetLinkedColumnSizeLimit(provider->GetDistanceMetric(
          views::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH))
      .AddRows(1, views::TableLayout::kFixedSize);
  return view;
}

std::u16string GetAnnotationTextForSetting(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_BLOCKED_AUX_TEXT);
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOWED_AUX_TEXT);
    case CONTENT_SETTING_SESSION_ONLY:
      return l10n_util::GetStringUTF16(
          IDS_COLLECTED_COOKIES_CLEAR_ON_EXIT_AUX_TEXT);
    default:
      NOTREACHED() << "Unknown ContentSetting value: " << setting;
      return std::u16string();
  }
}

// Creates a new CookiesTreeModel for all objects in the container,
// copying each of them.
std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModel(
    const browsing_data::LocalSharedObjectsContainer& shared_objects) {
  return std::make_unique<CookiesTreeModel>(
      LocalDataContainer::CreateFromLocalSharedObjectsContainer(shared_objects),
      /*special_storage_policy=*/nullptr);
}

PageSpecificSiteDataDialogAction GetDeleteActionForNodeType(
    CookieTreeNode::DetailedInfo::NodeType node_type) {
  switch (node_type) {
    // User deleted data at site level.
    case CookieTreeNode::DetailedInfo::TYPE_HOST:
      return PageSpecificSiteDataDialogAction::kSiteDeleted;
    // User deleted a single cookie.
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
      return PageSpecificSiteDataDialogAction::kSingleCookieDeleted;
    // User deleted cookies folder.
    case CookieTreeNode::DetailedInfo::TYPE_COOKIES:
      return PageSpecificSiteDataDialogAction::kCookiesFolderDeleted;
    // User deleted other folders.
    case CookieTreeNode::DetailedInfo::TYPE_DATABASES:
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGES:
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGES:
    case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DBS:
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEMS:
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKERS:
    case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER:
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKERS:
    case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER:
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGES:
    case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE:
      return PageSpecificSiteDataDialogAction::kFolderDeleted;
    case CookieTreeNode::DetailedInfo::TYPE_NONE:
    case CookieTreeNode::DetailedInfo::TYPE_ROOT:
      NOTREACHED()
          << "This node type is not visible to the user in UI. Node Type: "
          << node_type;
      return PageSpecificSiteDataDialogAction::kMaxValue;
  }
}

}  // namespace

// This DrawingProvider allows TreeModelNodes to be annotated with auxiliary
// text. Annotated nodes will be drawn in a lighter color than normal to
// indicate that their state has changed, and will have their auxiliary text
// drawn on the trailing end of their row.
class CookiesTreeViewDrawingProvider : public views::TreeViewDrawingProvider {
 public:
  CookiesTreeViewDrawingProvider() = default;
  ~CookiesTreeViewDrawingProvider() override {}

  void AnnotateNode(ui::TreeModelNode* node, const std::u16string& text);

  SkColor GetTextColorForNode(views::TreeView* tree_view,
                              ui::TreeModelNode* node) override;
  SkColor GetAuxiliaryTextColorForNode(views::TreeView* tree_view,
                                       ui::TreeModelNode* node) override;
  std::u16string GetAuxiliaryTextForNode(views::TreeView* tree_view,
                                         ui::TreeModelNode* node) override;
  bool ShouldDrawIconForNode(views::TreeView* tree_view,
                             ui::TreeModelNode* node) override;

 private:
  std::map<ui::TreeModelNode*, std::u16string> annotations_;
};

void CookiesTreeViewDrawingProvider::AnnotateNode(ui::TreeModelNode* node,
                                                  const std::u16string& text) {
  annotations_[node] = text;
}

SkColor CookiesTreeViewDrawingProvider::GetTextColorForNode(
    views::TreeView* tree_view,
    ui::TreeModelNode* node) {
  SkColor color = TreeViewDrawingProvider::GetTextColorForNode(tree_view, node);
  if (annotations_.find(node) != annotations_.end())
    color = SkColorSetA(color, 0x80);
  return color;
}

SkColor CookiesTreeViewDrawingProvider::GetAuxiliaryTextColorForNode(
    views::TreeView* tree_view,
    ui::TreeModelNode* node) {
  SkColor color = TreeViewDrawingProvider::GetTextColorForNode(tree_view, node);
  return SkColorSetA(color, 0x80);
}

std::u16string CookiesTreeViewDrawingProvider::GetAuxiliaryTextForNode(
    views::TreeView* tree_view,
    ui::TreeModelNode* node) {
  if (annotations_.find(node) != annotations_.end())
    return annotations_[node];

  CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
  if (cookie_node->GetDetailedInfo().node_type ==
          CookieTreeNode::DetailedInfo::TYPE_COOKIE &&
      cookie_node->GetDetailedInfo().cookie->IsPartitioned()) {
    return l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_PARTITIONED_COOKIE);
  }

  return TreeViewDrawingProvider::GetAuxiliaryTextForNode(tree_view, node);
}

bool CookiesTreeViewDrawingProvider::ShouldDrawIconForNode(
    views::TreeView* tree_view,
    ui::TreeModelNode* node) {
  CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
  return cookie_node->GetDetailedInfo().node_type !=
         CookieTreeNode::DetailedInfo::TYPE_HOST;
}

// A custom view that conditionally displays an infobar.
class InfobarView : public views::View {
 public:
  METADATA_HEADER(InfobarView);
  InfobarView() {
    info_image_ = AddChildView(std::make_unique<views::ImageView>());
    info_image_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kInfoOutlineIcon, ui::kColorIcon, 16));
    label_ = AddChildView(std::make_unique<views::Label>());

    const int vertical_distance =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
    const int horizontal_spacing =
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);

    // The containing dialog content view has no margins so that its
    // TabbedPane can span the full width of the dialog, but because of
    // that, InfobarView needs to impose its own horizontal margin.
    gfx::Insets insets =
        ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG);
    insets.set_top(vertical_distance);
    insets.set_bottom(vertical_distance);
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, insets,
        horizontal_spacing));
    SetVisible(false);
  }
  InfobarView(const InfobarView&) = delete;
  InfobarView& operator=(const InfobarView&) = delete;
  ~InfobarView() override = default;

  // Set the InfobarView label text based on content |setting| and
  // |domain_name|. Ensure InfobarView is visible.
  void SetLabelText(ContentSetting setting, const std::u16string& domain_name) {
    std::u16string label;
    switch (setting) {
      case CONTENT_SETTING_BLOCK:
        label = l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_BLOCK_RULE_CREATED, domain_name);
        break;

      case CONTENT_SETTING_ALLOW:
        label = l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_ALLOW_RULE_CREATED, domain_name);
        break;

      case CONTENT_SETTING_SESSION_ONLY:
        label = l10n_util::GetStringFUTF16(
            IDS_COLLECTED_COOKIES_SESSION_RULE_CREATED, domain_name);
        break;

      default:
        NOTREACHED();
    }
    label_->SetText(label);
    SetVisible(true);
  }

 private:
  // Info icon image.
  raw_ptr<views::ImageView> info_image_;
  // The label responsible for rendering the text.
  raw_ptr<views::Label> label_;
};

BEGIN_METADATA(InfobarView, views::View)
END_METADATA

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, public:

CollectedCookiesViews::~CollectedCookiesViews() {
  allowed_cookies_tree_->SetModel(nullptr);
  blocked_cookies_tree_->SetModel(nullptr);
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::TabbedPaneListener implementation:

void CollectedCookiesViews::TabSelectedAt(int index) {
  EnableControls();
  ShowCookieInfo();

  allowed_buttons_pane_->SetVisible(index == 0);
  blocked_buttons_pane_->SetVisible(index == 1);
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::TreeViewController implementation:

void CollectedCookiesViews::OnTreeViewSelectionChanged(
    views::TreeView* tree_view) {
  EnableControls();
  ShowCookieInfo();
}

///////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, views::View overrides:

gfx::Size CollectedCookiesViews::GetMinimumSize() const {
  // Allow UpdateWebContentsModalDialogPosition to clamp the dialog width.
  return gfx::Size(0, View::GetMinimumSize().height());
}

////////////////////////////////////////////////////////////////////////////////
// CollectedCookiesViews, private:

CollectedCookiesViews::CollectedCookiesViews(content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_DONE));
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  SetTitle(IDS_COLLECTED_COOKIES_DIALOG_TITLE);
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(gfx::Insets::TLBR(
          provider->GetDistanceMetric(
              views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
          0,
          provider->GetDistanceMetric(
              views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
          0));

  SetAcceptCallback(base::BindOnce(&CollectedCookiesViews::OnDialogClosed,
                                   base::Unretained(this)));
  SetCloseCallback(base::BindOnce(&CollectedCookiesViews::OnDialogClosed,
                                  base::Unretained(this)));

  views::TabbedPane* tabbed_pane =
      AddChildView(std::make_unique<views::TabbedPane>());

  // NOTE: Panes must be added after |tabbed_pane| has been added to its parent.
  std::u16string label_allowed = l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_TAB_LABEL);
  std::u16string label_blocked = l10n_util::GetStringUTF16(
      IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_TAB_LABEL);
  tabbed_pane->AddTab(label_allowed, CreateAllowedPane());
  tabbed_pane->AddTab(label_blocked, CreateBlockedPane());
  tabbed_pane->SelectTabAt(0);
  tabbed_pane->set_listener(this);
  tabbed_pane->SetProperty(views::kElementIdentifierKey, kTabbedPaneElementId);

  cookie_info_view_ = AddChildView(std::make_unique<CookieInfoView>());
  // Fix the height of the cookie info view, which is scrollable. It needs to be
  // large enough to fit at least 3-4 lines of information, but small enough
  // that it doesn't make the dialog too tall to fit in a small-ish browser.
  // (This is an accessibility issue; low-vision users using a high DPI zoom may
  // have browser windows under 600dip tall.)
  // TODO(pkasting): Could we clip to the browser window height (minus the size
  // of everything else)?
  cookie_info_view_->ClipHeightTo(kInfoViewHeight, kInfoViewHeight);

  // Always reserve space for the infobar, since there's no way to resize the
  // dialog larger to account for it dynamically. Unfortunately, FlexLayout
  // currently has no way to mark an invisible view as "should not be ignored by
  // layout". Instead, use an always-visible container view around the infobar,
  // relying on the default behavior of FillLayout -- to account for invisible
  // child views -- to size the container equal to the infobar's preferred size.
  auto* infobar_container = AddChildView(std::make_unique<views::View>());
  infobar_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  infobar_ = infobar_container->AddChildView(std::make_unique<InfobarView>());

  SetExtraView(CreateButtonsPane());

  constrained_window::ShowWebModalDialogViews(this, web_contents);

  EnableControls();
  ShowCookieInfo();
  RecordPageSpecificSiteDataDialogAction(
      PageSpecificSiteDataDialogAction::kDialogOpened);
}

void CollectedCookiesViews::OnDialogClosed() {
  // If the user closes our parent tab while we're still open, this method will
  // (eventually) be called in response to a WebContentsDestroyed() call from
  // the WebContentsImpl to its observers.  But since the
  // infobars::ContentInfoBarManager is also torn down in response to
  // WebContentsDestroyed(), it may already be null. Since the tab is going away
  // anyway, we can just omit showing an infobar, which prevents any attempt to
  // access a null infobars::ContentInfoBarManager. Same applies to removing the
  // webcontents' user data.
  if (!web_contents_ || web_contents_->IsBeingDestroyed())
    return;

  if (status_changed_)
    CollectedCookiesInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(web_contents_.get()));

  // Reset the dialog reference in the user data. If the dialog is opened again,
  // a new instance should be created. When the dialog is destroyed because of
  // the web contents being destroyed, no need to remove the user data because
  // it will be destroyed.
  web_contents_->RemoveUserData(
      PageSpecificSiteDataDialogController::UserDataKey());
}

std::unique_ptr<views::View> CollectedCookiesViews::CreateAllowedPane() {
  const auto* provider = ChromeLayoutProvider::Get();
  auto pane = std::make_unique<views::View>();
  pane->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  allowed_label_ = pane->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  allowed_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // This captures a snapshot of the allowed cookies of the current page so we
  // are fine using WebContents::GetPrimaryMainFrame() here
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents_->GetPrimaryMainFrame());
  allowed_cookies_tree_model_ =
      CreateCookiesTreeModel(content_settings->allowed_local_shared_objects());
  std::unique_ptr<CookiesTreeViewDrawingProvider> allowed_drawing_provider =
      std::make_unique<CookiesTreeViewDrawingProvider>();
  allowed_cookies_drawing_provider_ = allowed_drawing_provider.get();
  auto allowed_cookies_tree = std::make_unique<views::TreeView>();
  allowed_cookies_tree->SetModel(allowed_cookies_tree_model_.get());
  allowed_cookies_tree->SetDrawingProvider(std::move(allowed_drawing_provider));
  allowed_cookies_tree->SetRootShown(false);
  allowed_cookies_tree->SetEditable(false);
  allowed_cookies_tree->set_auto_expand_children(true);
  allowed_cookies_tree->SetController(this);
  allowed_cookies_tree->SetProperty(views::kElementIdentifierKey,
                                    kAllowedCookiesTreeElementId);
  allowed_cookies_tree_ = allowed_cookies_tree.get();
  auto* scroll_view =
      pane->AddChildView(CreateScrollView(std::move(allowed_cookies_tree)));
  scroll_view->SetPreferredSize(gfx::Size(kTreeViewWidth, kTreeViewHeight));

  return pane;
}

std::unique_ptr<views::View> CollectedCookiesViews::CreateBlockedPane() {
  const auto* provider = ChromeLayoutProvider::Get();
  auto pane = std::make_unique<views::View>();
  pane->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  blocked_label_ = pane->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          cookie_settings->ShouldBlockThirdPartyCookies()
              ? IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED
              : IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  blocked_label_->SetMultiLine(true);
  blocked_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  blocked_label_->SizeToFit(kTreeViewWidth);

  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents_->GetPrimaryMainFrame());
  blocked_cookies_tree_model_ =
      CreateCookiesTreeModel(content_settings->blocked_local_shared_objects());
  std::unique_ptr<CookiesTreeViewDrawingProvider> blocked_drawing_provider =
      std::make_unique<CookiesTreeViewDrawingProvider>();
  blocked_cookies_drawing_provider_ = blocked_drawing_provider.get();
  auto blocked_cookies_tree = std::make_unique<views::TreeView>();
  blocked_cookies_tree->SetModel(blocked_cookies_tree_model_.get());
  blocked_cookies_tree->SetDrawingProvider(std::move(blocked_drawing_provider));
  blocked_cookies_tree->SetRootShown(false);
  blocked_cookies_tree->SetEditable(false);
  blocked_cookies_tree->set_auto_expand_children(true);
  blocked_cookies_tree->SetController(this);
  blocked_cookies_tree->SetProperty(views::kElementIdentifierKey,
                                    kBlockedCookiesTreeElementId);
  blocked_cookies_tree_ = blocked_cookies_tree.get();
  auto* scroll_view =
      pane->AddChildView(CreateScrollView(std::move(blocked_cookies_tree)));
  scroll_view->SetPreferredSize(gfx::Size(kTreeViewWidth, kTreeViewHeight));

  return pane;
}

std::unique_ptr<views::View> CollectedCookiesViews::CreateButtonsPane() {
  auto view = std::make_unique<views::View>();
  view->SetUseDefaultFillLayout(true);

  {
    auto allowed = CreateNewButtonView();
    block_allowed_button_ =
        allowed->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&CollectedCookiesViews::AddContentException,
                                base::Unretained(this), allowed_cookies_tree_,
                                CONTENT_SETTING_BLOCK),
            l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_BLOCK_BUTTON)));

    block_allowed_button_->SetProperty(views::kElementIdentifierKey,
                                       kBlockButtonId);
    delete_allowed_button_ =
        allowed->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(
                &CollectedCookiesViews::DeleteSelectedCookieNode,
                base::Unretained(this)),
            l10n_util::GetStringUTF16(IDS_COOKIES_REMOVE_LABEL)));

    delete_allowed_button_->SetProperty(views::kElementIdentifierKey,
                                        kRemoveButtonId);
    allowed_buttons_pane_ = view->AddChildView(std::move(allowed));
  }

  {
    auto blocked = CreateNewButtonView();
    blocked->SetVisible(false);
    allow_blocked_button_ =
        blocked->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&CollectedCookiesViews::AddContentException,
                                base::Unretained(this), blocked_cookies_tree_,
                                CONTENT_SETTING_ALLOW),
            l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_ALLOW_BUTTON)));
    allow_blocked_button_->SetProperty(views::kElementIdentifierKey,
                                       kAllowButtonId);
    for_session_blocked_button_ =
        blocked->AddChildView(std::make_unique<views::MdTextButton>(
            base::BindRepeating(&CollectedCookiesViews::AddContentException,
                                base::Unretained(this), blocked_cookies_tree_,
                                CONTENT_SETTING_SESSION_ONLY),
            l10n_util::GetStringUTF16(
                IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON)));
    for_session_blocked_button_->SetProperty(views::kElementIdentifierKey,
                                             kClearOnExitButtonId);
    blocked_buttons_pane_ = view->AddChildView(std::move(blocked));
  }

  return view;
}

std::unique_ptr<views::View> CollectedCookiesViews::CreateScrollView(
    std::unique_ptr<views::TreeView> pane) {
  auto scroll_view = views::ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(std::move(pane));
  return scroll_view;
}

void CollectedCookiesViews::EnableControls() {
  bool enable_allowed_buttons = false;
  ui::TreeModelNode* node = allowed_cookies_tree_->GetSelectedNode();
  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_HOST) {
      enable_allowed_buttons = static_cast<CookieTreeHostNode*>(
          cookie_node)->CanCreateContentException();
    }
  }
  block_allowed_button_->SetEnabled(enable_allowed_buttons);
  delete_allowed_button_->SetEnabled(node != nullptr);

  bool enable_blocked_buttons = false;
  node = blocked_cookies_tree_->GetSelectedNode();
  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_HOST) {
      enable_blocked_buttons = static_cast<CookieTreeHostNode*>(
          cookie_node)->CanCreateContentException();
    }
  }
  allow_blocked_button_->SetEnabled(enable_blocked_buttons);
  for_session_blocked_button_->SetEnabled(enable_blocked_buttons);
}

void CollectedCookiesViews::ShowCookieInfo() {
  ui::TreeModelNode* node = allowed_cookies_tree_->IsDrawn() ?
                            allowed_cookies_tree_->GetSelectedNode() : nullptr;

  if (!node && blocked_cookies_tree_->IsDrawn())
    node = blocked_cookies_tree_->GetSelectedNode();

  if (node) {
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    const CookieTreeNode::DetailedInfo detailed_info =
        cookie_node->GetDetailedInfo();

    if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
      cookie_info_view_->SetCookie(detailed_info.cookie->Domain(),
                                   *detailed_info.cookie);
    } else {
      cookie_info_view_->ClearCookieDisplay();
    }
  } else {
    cookie_info_view_->ClearCookieDisplay();
  }
}

void CollectedCookiesViews::AddContentException(views::TreeView* tree_view,
                                                ContentSetting setting) {
  CookieTreeHostNode* host_node =
      static_cast<CookieTreeHostNode*>(tree_view->GetSelectedNode());
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  host_node->CreateContentException(
      CookieSettingsFactory::GetForProfile(profile).get(), setting);
  infobar_->SetLabelText(setting, host_node->GetTitle());
  status_changed_ = true;

  RecordPageSpecificSiteDataDialogAction(
      GetDialogActionForContentSetting(setting));

  CookiesTreeViewDrawingProvider* provider =
      (tree_view == allowed_cookies_tree_)
          ? allowed_cookies_drawing_provider_.get()
          : blocked_cookies_drawing_provider_.get();
  provider->AnnotateNode(tree_view->GetSelectedNode(),
                         GetAnnotationTextForSetting(setting));
  tree_view->SchedulePaint();
}

void CollectedCookiesViews::DeleteSelectedCookieNode() {
  CookieTreeNode* cookie_node =
      static_cast<CookieTreeNode*>(allowed_cookies_tree_->GetSelectedNode());
  CookieTreeNode::DetailedInfo::NodeType node_type =
      cookie_node->GetDetailedInfo().node_type;

  allowed_cookies_tree_model_->DeleteCookieNode(cookie_node);
  RecordPageSpecificSiteDataDialogAction(GetDeleteActionForNodeType(node_type));
}

BEGIN_METADATA(CollectedCookiesViews, views::DialogDelegateView)
END_METADATA
