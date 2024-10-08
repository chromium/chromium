// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/content_setting_site_row_view.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

using ManageTextStyle = ContentSettingBubbleModel::ManageTextStyle;

// Helper functions to access BubbleContent attributes that depend on user
// modifiable state.
std::u16string GetDoneButtonText(
    const ContentSettingBubbleModel::BubbleContent& content) {
  return content.is_user_modifiable
             ? content.done_button_text
             : l10n_util::GetStringUTF16(IDS_SETTINGS_GOT_IT);
}

std::u16string GetCancelButtonText(
    const ContentSettingBubbleModel::BubbleContent& content) {
  return content.is_user_modifiable ? content.cancel_button_text
                                    : std::u16string();
}

ui::ImageModel GetSiteSettingsIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kSettingsChromeRefreshIcon, ui::kColorIcon,
      GetLayoutConstant(PAGE_INFO_ICON_SIZE));
}

ui::ImageModel GetLaunchIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kLaunchChromeRefreshIcon,
                                        ui::kColorIcon,
                                        GetLayoutConstant(PAGE_INFO_ICON_SIZE));
}

bool ShouldShowManageButton(
    const ContentSettingBubbleModel::BubbleContent& content) {
  return content.manage_text_style ==
             ContentSettingBubbleModel::ManageTextStyle::kButton &&
         content.is_user_modifiable;
}

enum class LayoutRowType {
  DEFAULT,
  INDENTED,
  FULL_WIDTH,
};

// Represents a row containing a single view in the layout. The type determines
// the view's horizontal margins.
struct LayoutRow {
  std::unique_ptr<views::View> view;
  LayoutRowType type;
};

}  // namespace

// ContentSettingBubbleContents::ListItemContainer -----------------------------

class ContentSettingBubbleContents::ListItemContainer : public views::View {
  METADATA_HEADER(ListItemContainer, views::View)

 public:
  explicit ListItemContainer(ContentSettingBubbleContents* parent);
  ListItemContainer(const ListItemContainer&) = delete;
  ListItemContainer& operator=(const ListItemContainer&) = delete;

  // Creates and adds child views representing |item|.
  void AddItem(const ContentSettingBubbleModel::ListItem& item);

  // Calling this will delete related children.
  void RemoveRowAtIndex(int index);

 private:
  using Row = std::pair<views::ImageView*, views::View*>;
  using NewRow = std::pair<std::unique_ptr<views::ImageView>,
                           std::unique_ptr<views::View>>;

  void ResetLayout();
  void AddRowToLayout(const Row& row);
  Row AddNewRowToLayout(NewRow row);
  void UpdateScrollHeight(const Row& row);

  raw_ptr<ContentSettingBubbleContents> parent_;

  // Our controls representing list items, so we can add or remove
  // these dynamically. Each pair represents one list item.
  std::vector<Row> list_item_views_;
};

ContentSettingBubbleContents::ListItemContainer::ListItemContainer(
    ContentSettingBubbleContents* parent)
    : parent_(parent) {
  ResetLayout();
}

void ContentSettingBubbleContents::ListItemContainer::AddItem(
    const ContentSettingBubbleModel::ListItem& item) {
  // Padding for list items and icons.
  static constexpr auto kTitleDescriptionListItemInset =
      gfx::Insets::TLBR(3, 0, 13, 0);

  auto item_icon = std::make_unique<views::ImageView>();
  if (item.image) {
    item_icon->SetBorder(
        views::CreateEmptyBorder(kTitleDescriptionListItemInset));
    item_icon->SetImage(ui::ImageModel::FromVectorIcon(
        *item.image, ui::kColorLabelForeground,
        GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
        item.has_blocked_badge ? &vector_icons::kBlockedBadgeIcon
                               : &gfx::kNoneIcon));
  }

  std::unique_ptr<views::View> item_contents;
  if (item.has_link) {
    auto link = std::make_unique<views::Link>(item.title);
    link->SetElideBehavior(gfx::ELIDE_MIDDLE);
    link->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    link->SetCallback(base::BindRepeating(
        [](const std::vector<Row>* items, const views::Link* link,
           ContentSettingBubbleContents* parent, const ui::Event& event) {
          const auto it = base::ranges::find(*items, link, &Row::second);
          DCHECK(it != items->cend());
          parent->LinkClicked(std::distance(items->cbegin(), it), event);
        },
        base::Unretained(&list_item_views_), base::Unretained(link.get()),
        base::Unretained(parent_)));
    item_contents = std::move(link);
  } else {
    item_contents = std::make_unique<views::View>();
    item_contents->SetBorder(
        views::CreateEmptyBorder(kTitleDescriptionListItemInset));
    item_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    const auto add_label = [&item_contents](const std::u16string& string,
                                            int style) {
      if (!string.empty()) {
        auto label = std::make_unique<views::Label>(
            string, views::style::CONTEXT_LABEL, style);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        label->SetAllowCharacterBreak(true);
        item_contents->AddChildView(std::move(label));
      }
    };
    add_label(item.title, views::style::STYLE_PRIMARY);
    add_label(item.description, views::style::STYLE_DISABLED);
  }

  list_item_views_.push_back(AddNewRowToLayout(
      NewRow(std::move(item_icon), std::move(item_contents))));
}

void ContentSettingBubbleContents::ListItemContainer::RemoveRowAtIndex(
    int index) {
  auto& children = list_item_views_[index];
  delete children.first;
  delete children.second;
  list_item_views_.erase(list_item_views_.begin() + index);

  // As TableLayout can't remove rows, we have to rebuild it entirely.
  ResetLayout();
  for (auto& row : list_item_views_)
    AddRowToLayout(row);
}

void ContentSettingBubbleContents::ListItemContainer::ResetLayout() {
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kStretch,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL))
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStretch, 1.0,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  auto* scroll_view = views::ScrollView::GetScrollViewForContents(this);
  // When this function is called from the constructor, the view has not yet
  // been placed into a ScrollView.
  if (scroll_view)
    scroll_view->ClipHeightTo(-1, -1);
}

void ContentSettingBubbleContents::ListItemContainer::AddRowToLayout(
    const Row& row) {
  static_cast<views::TableLayout*>(GetLayoutManager())
      ->AddRows(1, views::TableLayout::kFixedSize, 0);
  UpdateScrollHeight(row);
}

ContentSettingBubbleContents::ListItemContainer::Row
ContentSettingBubbleContents::ListItemContainer::AddNewRowToLayout(NewRow row) {
  static_cast<views::TableLayout*>(GetLayoutManager())
      ->AddRows(1, views::TableLayout::kFixedSize);
  Row row_result;
  row_result.first = AddChildView(std::move(row.first));
  row_result.second = AddChildView(std::move(row.second));
  UpdateScrollHeight(row_result);
  return row_result;
}

void ContentSettingBubbleContents::ListItemContainer::UpdateScrollHeight(
    const Row& row) {
  auto* scroll_view = views::ScrollView::GetScrollViewForContents(this);
  DCHECK(scroll_view);
  if (!scroll_view->is_bounded()) {
    // Display a maximum of 4 visible items in a list before scrolling.
    static constexpr int kMaxVisibleListItems = 4;
    scroll_view->ClipHeightTo(
        0, std::max(row.first->GetPreferredSize().height(),
                    row.second->GetPreferredSize().height()) *
               kMaxVisibleListItems);
  }
}

BEGIN_METADATA(ContentSettingBubbleContents, ListItemContainer)
END_METADATA

// ContentSettingBubbleContents -----------------------------------------------

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContentSettingBubbleContents,
                                      kMainElementId);

ContentSettingBubbleContents::ContentSettingBubbleContents(
    std::unique_ptr<ContentSettingBubbleModel> content_setting_bubble_model,
    content::WebContents* web_contents,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : content::WebContentsObserver(web_contents),
      BubbleDialogDelegateView(anchor_view,
                               arrow,
                               views::BubbleBorder::DIALOG_SHADOW,
                               true),
      content_setting_bubble_model_(std::move(content_setting_bubble_model)) {
  // Although other code in this class treats content_setting_bubble_model_ as
  // though it's optional, in fact it can only become null if
  // WebContentsDestroyed() is called, which can't happen until the constructor
  // has run - so it is never null here.
  DCHECK(content_setting_bubble_model_);
  const std::u16string& done_text =
      GetDoneButtonText(content_setting_bubble_model_->bubble_content());
  const std::u16string& cancel_text =
      GetCancelButtonText(content_setting_bubble_model_->bubble_content());
  SetButtons(cancel_text.empty()
                 ? static_cast<int>(ui::mojom::DialogButton::kOk)
                 : static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      done_text.empty() ? l10n_util::GetStringUTF16(IDS_DONE) : done_text);
  SetExtraView(CreateHelpAndManageView());
  SetAcceptCallback(
      base::BindOnce(&ContentSettingBubbleModel::OnDoneButtonClicked,
                     base::Unretained(content_setting_bubble_model_.get())));

  if (!cancel_text.empty()) {
    SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_text);
    SetCancelCallback(
        base::BindOnce(&ContentSettingBubbleModel::OnCancelButtonClicked,
                       base::Unretained(content_setting_bubble_model_.get())));
  }

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  SetProperty(views::kElementIdentifierKey, kMainElementId);
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
  // Must remove the children here so the comboboxes get destroyed before
  // their associated models.
  RemoveAllChildViews();
}

void ContentSettingBubbleContents::WindowClosing() {
  if (content_setting_bubble_model_) {
    content_setting_bubble_model_->CommitChanges();
  }
}

void ContentSettingBubbleContents::OnListItemAdded(
    const ContentSettingBubbleModel::ListItem& item) {
  DCHECK(list_item_container_);
  list_item_container_->AddItem(item);
}

void ContentSettingBubbleContents::OnListItemRemovedAt(int index) {
  DCHECK(list_item_container_);
  list_item_container_->RemoveRowAtIndex(index);
}

int ContentSettingBubbleContents::GetSelectedRadioOption() {
  for (RadioGroup::const_iterator i(radio_group_.begin());
       i != radio_group_.end(); ++i) {
    if ((*i)->GetChecked())
      return i - radio_group_.begin();
  }
  NOTREACHED();
}

void ContentSettingBubbleContents::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  if (learn_more_button_)
    StyleLearnMoreButton();
}

std::u16string ContentSettingBubbleContents::GetWindowTitle() const {
  if (!content_setting_bubble_model_)
    return std::u16string();
  return content_setting_bubble_model_->bubble_content().title;
}

bool ContentSettingBubbleContents::ShouldShowCloseButton() const {
  return true;
}

void ContentSettingBubbleContents::Init() {
  DCHECK(content_setting_bubble_model_);
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  int vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      vertical_spacing));
  std::vector<LayoutRow> rows;

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();

  if (!bubble_content.subtitle.empty()) {
    SetSubtitle(bubble_content.subtitle);
    auto separator = std::make_unique<views::Separator>();
    separator->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(vertical_spacing, 0));
    rows.push_back({std::move(separator), LayoutRowType::FULL_WIDTH});
  }

  if (!bubble_content.message.empty()) {
    auto message_label = std::make_unique<views::Label>(
        bubble_content.message, views::style::CONTEXT_LABEL,
        views::style::STYLE_BODY_3);
    message_label->SetEnabledColorId(kColorActivityIndicatorForeground);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rows.push_back({std::move(message_label), LayoutRowType::DEFAULT});
  }

  // Layout for the item list (blocked plugins and popups).
  if (!bubble_content.list_items.empty()) {
    auto list_item_container = std::make_unique<ListItemContainer>(this);
    list_item_container->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, margins().left(), 0, margins().right())));
    auto scroll_view = std::make_unique<views::ScrollView>();
    list_item_container_ =
        scroll_view->SetContents(std::move(list_item_container));
    rows.push_back({std::move(scroll_view), LayoutRowType::FULL_WIDTH});

    for (const ContentSettingBubbleModel::ListItem& list_item :
         bubble_content.list_items) {
      list_item_container_->AddItem(list_item);
    }
  }

  const ContentSettingBubbleModel::RadioGroup& radio_group =
      bubble_content.radio_group;
  if (!radio_group.radio_items.empty()) {
    for (const auto& radio_item : radio_group.radio_items) {
      auto radio = std::make_unique<views::RadioButton>(radio_item, 0);
      radio->SetLabelStyle(views::style::STYLE_BODY_4);
      radio->SetEnabledTextColorIds(kColorActivityIndicatorSubtitleForeground);
      radio->SetVisible(bubble_content.is_user_modifiable);
      radio->SetMultiLine(true);
      radio_group_.push_back(radio.get());
      rows.push_back({std::move(radio), LayoutRowType::INDENTED});
    }
    DCHECK(!radio_group_.empty());
    // Now that the buttons have been added to the view hierarchy, it's safe
    // to call SetChecked() on them.
    radio_group_[radio_group.default_item]->SetChecked(true);
  }

  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* favicon_service = FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!bubble_content.site_list.empty()) {
    // Put site rows into a separate container to avoid additional space between
    // these rows.
    auto sites_container = std::make_unique<views::View>();
    sites_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    for (const auto& entry : bubble_content.site_list) {
      auto site_row = std::make_unique<ContentSettingSiteRowView>(
          favicon_service, entry.first, entry.second,
          base::BindRepeating(
              &ContentSettingBubbleModel::OnSiteRowClicked,
              base::Unretained(content_setting_bubble_model_.get())));
      sites_container->AddChildView(std::move(site_row));
    }
    rows.push_back({std::move(sites_container), LayoutRowType::FULL_WIDTH});
  }

  if (!bubble_content.custom_link.empty()) {
    auto custom_link =
        std::make_unique<views::Link>(bubble_content.custom_link);
    custom_link->SetEnabled(bubble_content.custom_link_enabled);
    custom_link->SetMultiLine(true);
    custom_link->SetCallback(
        base::BindRepeating(&ContentSettingBubbleContents::CustomLinkClicked,
                            base::Unretained(this)));
    custom_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rows.push_back({std::move(custom_link), LayoutRowType::DEFAULT});
  }

  if (bubble_content.manage_text_style == ManageTextStyle::kCheckbox) {
    auto manage_checkbox = std::make_unique<views::Checkbox>(
        bubble_content.manage_text,
        base::BindRepeating(
            [](ContentSettingBubbleContents* bubble) {
              bubble->content_setting_bubble_model_->OnManageCheckboxChecked(
                  bubble->manage_checkbox_->GetChecked());
              // Toggling the check state may change the dialog button text.
              bubble->DialogModelChanged();
            },
            base::Unretained(this)));
    manage_checkbox_ = manage_checkbox.get();
    rows.push_back({std::move(manage_checkbox), LayoutRowType::DEFAULT});
  }

  if (bubble_content.manage_text_style == ManageTextStyle::kHoverButton) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    auto separator = std::make_unique<views::Separator>();
    rows.push_back({std::move(separator), LayoutRowType::DEFAULT});

    auto site_settings_link = std::make_unique<RichHoverButton>(
        base::BindRepeating(
            [](ContentSettingBubbleContents* bubble) {
              bubble->GetWidget()->Close();
              bubble->content_setting_bubble_model_->OnManageButtonClicked();
            },
            this),
        GetSiteSettingsIcon(), bubble_content.manage_text,
        /*secondary_text=*/std::u16string(), bubble_content.manage_tooltip,
        /*subtitle_text=*/std::u16string(), GetLaunchIcon());
    rows.push_back({std::move(site_settings_link), LayoutRowType::FULL_WIDTH});
  }

  // We have to apply the left and right margins manually, because rows using
  // LayoutRowType::FULL_WIDTH need to not have them applied to look correct.
  const int left_margin = margins().left();
  const int right_margin = margins().right();

  int bottom_margin = margins().bottom();
  if (bubble_content.manage_text_style == ManageTextStyle::kHoverButton) {
    bottom_margin =
        provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  }

  set_margins(gfx::Insets::TLBR(margins().top(), 0, bottom_margin, 0));

  for (LayoutRow& row : rows) {
    if (row.type != LayoutRowType::FULL_WIDTH) {
      const int row_left_margin =
          left_margin + (row.type == LayoutRowType::INDENTED
                             ? provider->GetDistanceMetric(
                                   DISTANCE_SUBSECTION_HORIZONTAL_INDENT)
                             : 0);
      row.view->SetBorder(views::CreateEmptyBorder(
          gfx::Insets::TLBR(0, row_left_margin, 0, right_margin)));
    }
    AddChildView(std::move(row.view));
  }

  content_setting_bubble_model_->set_owner(this);
}

void ContentSettingBubbleContents::StyleLearnMoreButton() {
  DCHECK(learn_more_button_);
  const ui::ColorProvider* cp = GetColorProvider();
  SkColor icon_color = cp->GetColor(ui::kColorIcon);
  SkColor icon_disabled_color = cp->GetColor(ui::kColorIconDisabled);
  views::SetImageFromVectorIconWithColor(learn_more_button_,
                                         vector_icons::kHelpOutlineIcon,
                                         icon_color, icon_disabled_color);
}

std::unique_ptr<views::View>
ContentSettingBubbleContents::CreateHelpAndManageView() {
  DCHECK(content_setting_bubble_model_);
  const auto& bubble_content = content_setting_bubble_model_->bubble_content();
  const auto* layout = ChromeLayoutProvider::Get();
  std::vector<std::unique_ptr<views::View>> extra_views;
  // Optionally add a help icon if the view wants to link to a help page.
  if (bubble_content.show_learn_more) {
    auto learn_more_button = views::CreateVectorImageButton(base::BindRepeating(
        [](ContentSettingBubbleContents* bubble) {
          bubble->GetWidget()->Close();
          bubble->content_setting_bubble_model_->OnLearnMoreClicked();
        },
        base::Unretained(this)));
    learn_more_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    learn_more_button_ = learn_more_button.get();
    extra_views.push_back(std::move(learn_more_button));
  }
  // Optionally add a "Manage" button if the view wants to use a button to
  // invoke a separate management UI related to the dialog content.
  if (ShouldShowManageButton(bubble_content)) {
    std::u16string title = bubble_content.manage_text;
    if (title.empty())
      title = l10n_util::GetStringUTF16(IDS_MANAGE);
    auto manage_button = std::make_unique<views::MdTextButton>(
        base::BindRepeating(
            [](ContentSettingBubbleContents* bubble) {
              bubble->GetWidget()->Close();
              bubble->content_setting_bubble_model_->OnManageButtonClicked();
            },
            base::Unretained(this)),
        title);
    manage_button->SetMinSize(gfx::Size(
        layout->GetDistanceMetric(views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH),
        0));
    manage_button->SetStyle(ui::ButtonStyle::kTonal);
    manage_button_ = manage_button.get();
    extra_views.push_back(std::move(manage_button));
  }
  if (extra_views.empty())
    return nullptr;
  if (extra_views.size() == 1)
    return std::move(extra_views.front());
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      layout->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  for (auto& extra_view : extra_views)
    container->AddChildView(std::move(extra_view));
  return container;
}

void ContentSettingBubbleContents::LinkClicked(int row,
                                               const ui::Event& event) {
  DCHECK(content_setting_bubble_model_);
  DCHECK_NE(row, -1);
  content_setting_bubble_model_->OnListItemClicked(row, event);
}

void ContentSettingBubbleContents::CustomLinkClicked() {
  DCHECK(content_setting_bubble_model_);
  content_setting_bubble_model_->OnCustomLinkClicked();
  GetWidget()->Close();
}

void ContentSettingBubbleContents::PrimaryPageChanged(content::Page& page) {
  // Content settings are based on the main frame, so if it switches then
  // close up shop.
  GetWidget()->Close();
}

void ContentSettingBubbleContents::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    GetWidget()->Close();
}

void ContentSettingBubbleContents::WebContentsDestroyed() {
  // Destroy the bubble model to ensure that the underlying WebContents outlives
  // it.
  content_setting_bubble_model_->CommitChanges();
  content_setting_bubble_model_.reset();

  // Closing the widget should synchronously hide it (and post a task to delete
  // it). Subsequent event listener methods should not be invoked on hidden
  // widgets.
  GetWidget()->Close();
}

BEGIN_METADATA(ContentSettingBubbleContents)
END_METADATA
