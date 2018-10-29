// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/content_setting_domain_list_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/native_cursor.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// Display a maximum of 4 visible items in a list before scrolling.
const int kMaxVisibleListItems = 4;

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

// A combobox model that builds the contents of the media capture devices menu
// in the content setting bubble.
class MediaComboboxModel : public ui::ComboboxModel {
 public:
  explicit MediaComboboxModel(content::MediaStreamType type);
  ~MediaComboboxModel() override;

  content::MediaStreamType type() const { return type_; }
  const content::MediaStreamDevices& GetDevices() const;
  int GetDeviceIndex(const content::MediaStreamDevice& device) const;

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;

 private:
  content::MediaStreamType type_;

  DISALLOW_COPY_AND_ASSIGN(MediaComboboxModel);
};

// A view representing one or more rows, each containing a label and combobox
// pair, that allow the user to select a device for each media type (microphone
// and/or camera).
class MediaMenuBlock : public views::View {
 public:
  MediaMenuBlock(views::ComboboxListener* listener,
                 ContentSettingBubbleModel::MediaMenuMap media) {
    const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>(this));
    constexpr int kColumnSetId = 0;
    views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(
        views::GridLayout::kFixedSize,
        provider->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                          views::GridLayout::FIXED, 0, 0);

    bool first_row = true;
    for (auto i = media.cbegin(); i != media.cend(); ++i) {
      if (!first_row) {
        layout->AddPaddingRow(views::GridLayout::kFixedSize,
                              provider->GetDistanceMetric(
                                  views::DISTANCE_RELATED_CONTROL_VERTICAL));
      }
      first_row = false;

      layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
      content::MediaStreamType stream_type = i->first;
      const ContentSettingBubbleModel::MediaMenu& menu = i->second;

      views::Label* label = new views::Label(menu.label);
      label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      layout->AddView(label);

      auto combobox_model = std::make_unique<MediaComboboxModel>(stream_type);
      // Disable the device selection when the website is managing the devices
      // itself or if there are no devices present.
      const bool combobox_enabled =
          !menu.disabled && !combobox_model->GetDevices().empty();
      const int combobox_selected_index =
          combobox_model->GetDevices().empty()
              ? 0
              : combobox_model->GetDeviceIndex(menu.selected_device);
      // The combobox takes ownership of the model.
      views::Combobox* combobox =
          new views::Combobox(std::move(combobox_model));
      combobox->SetEnabled(combobox_enabled);
      combobox->set_listener(listener);
      combobox->SetSelectedIndex(combobox_selected_index);
      layout->AddView(combobox);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaMenuBlock);
};

}  // namespace

// MediaComboboxModel ----------------------------------------------------------

MediaComboboxModel::MediaComboboxModel(content::MediaStreamType type)
    : type_(type) {
  DCHECK(type_ == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
         type_ == content::MEDIA_DEVICE_VIDEO_CAPTURE);
}

MediaComboboxModel::~MediaComboboxModel() {}

const content::MediaStreamDevices& MediaComboboxModel::GetDevices() const {
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  return type_ == content::MEDIA_DEVICE_AUDIO_CAPTURE
             ? dispatcher->GetAudioCaptureDevices()
             : dispatcher->GetVideoCaptureDevices();
}

int MediaComboboxModel::GetDeviceIndex(
    const content::MediaStreamDevice& device) const {
  const auto& devices = GetDevices();
  for (size_t i = 0; i < devices.size(); ++i) {
    if (device.id == devices[i].id)
      return i;
  }
  NOTREACHED();
  return 0;
}

int MediaComboboxModel::GetItemCount() const {
  return std::max(1, static_cast<int>(GetDevices().size()));
}

base::string16 MediaComboboxModel::GetItemAt(int index) {
  return GetDevices().empty()
             ? l10n_util::GetStringUTF16(IDS_MEDIA_MENU_NO_DEVICE_TITLE)
             : base::UTF8ToUTF16(GetDevices()[index].name);
}

// ContentSettingBubbleContents::Favicon --------------------------------------

class ContentSettingBubbleContents::Favicon : public views::ImageView {
 public:
  Favicon(const gfx::Image& image,
          ContentSettingBubbleContents* parent,
          views::Link* link);
  ~Favicon() override;

 private:
  // views::View overrides:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;

  ContentSettingBubbleContents* parent_;
  views::Link* link_;
};

ContentSettingBubbleContents::Favicon::Favicon(
    const gfx::Image& image,
    ContentSettingBubbleContents* parent,
    views::Link* link)
    : parent_(parent),
      link_(link) {
  SetImage(image.AsImageSkia());
}

ContentSettingBubbleContents::Favicon::~Favicon() {
}

bool ContentSettingBubbleContents::Favicon::OnMousePressed(
    const ui::MouseEvent& event) {
  return event.IsLeftMouseButton() || event.IsMiddleMouseButton();
}

void ContentSettingBubbleContents::Favicon::OnMouseReleased(
    const ui::MouseEvent& event) {
  if ((event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
     HitTestPoint(event.location())) {
    parent_->LinkClicked(link_, event.flags());
  }
}

gfx::NativeCursor ContentSettingBubbleContents::Favicon::GetCursor(
    const ui::MouseEvent& event) {
  return views::GetNativeHandCursor();
}

// ContentSettingBubbleContents::ListItemContainer -----------------------------

class ContentSettingBubbleContents::ListItemContainer : public views::View {
 public:
  explicit ListItemContainer(ContentSettingBubbleContents* parent);

  // Creates and adds child views representing |item|.
  void AddItem(const ContentSettingBubbleModel::ListItem& item);

  // Calling this will delete related children.
  void RemoveRowAtIndex(int index);

  // Returns row index of |link| among list items.
  int GetRowIndexOf(const views::Link* link) const;

 private:
  using Row = std::pair<views::ImageView*, views::Label*>;

  void ResetLayout();
  void AddRowToLayout(const Row& row);

  ContentSettingBubbleContents* parent_;

  // Our controls representing list items, so we can add or remove
  // these dynamically. Each pair represetns one list item.
  std::vector<Row> list_item_views_;

  DISALLOW_COPY_AND_ASSIGN(ListItemContainer);
};

ContentSettingBubbleContents::ListItemContainer::ListItemContainer(
    ContentSettingBubbleContents* parent)
    : parent_(parent) {
  ResetLayout();
}

void ContentSettingBubbleContents::ListItemContainer::AddItem(
    const ContentSettingBubbleModel::ListItem& item) {
  views::ImageView* icon = nullptr;
  views::Label* label = nullptr;
  if (item.has_link) {
    views::Link* link = new views::Link(item.title);
    link->set_listener(parent_);
    link->SetElideBehavior(gfx::ELIDE_MIDDLE);
    icon = new Favicon(item.image, parent_, link);
    label = link;
  } else {
    icon = new views::ImageView();
    icon->SetImage(item.image.AsImageSkia());
    label = new views::Label(item.title);
  }
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  list_item_views_.push_back(Row(icon, label));
  AddRowToLayout(list_item_views_.back());
}

void ContentSettingBubbleContents::ListItemContainer::RemoveRowAtIndex(
    int index) {
  auto& children = list_item_views_[index];
  delete children.first;
  delete children.second;
  list_item_views_.erase(list_item_views_.begin() + index);

  // As GridLayout can't remove rows, we have to rebuild it entirely.
  ResetLayout();
  for (size_t i = 0; i < list_item_views_.size(); i++)
    AddRowToLayout(list_item_views_[i]);
}

int ContentSettingBubbleContents::ListItemContainer::GetRowIndexOf(
    const views::Link* link) const {
  auto has_link = [link](const Row& row) { return row.second == link; };
  auto iter = std::find_if(list_item_views_.begin(), list_item_views_.end(),
                           has_link);
  return (iter == list_item_views_.end())
             ? -1
             : std::distance(list_item_views_.begin(), iter);
}

void ContentSettingBubbleContents::ListItemContainer::ResetLayout() {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* item_list_column_set = layout->AddColumnSet(0);
  item_list_column_set->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::FILL,
      views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  const int related_control_horizontal_spacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  item_list_column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                         related_control_horizontal_spacing);
  item_list_column_set->AddColumn(views::GridLayout::LEADING,
                                  views::GridLayout::FILL, 1.0,
                                  views::GridLayout::USE_PREF, 0, 0);
  auto* scroll_view = views::ScrollView::GetScrollViewForContents(this);
  // When this function is called from the constructor, the view has not yet
  // been placed into a ScrollView.
  if (scroll_view)
    scroll_view->ClipHeightTo(-1, -1);
}

void ContentSettingBubbleContents::ListItemContainer::AddRowToLayout(
    const Row& row) {
  views::GridLayout* layout =
      static_cast<views::GridLayout*>(GetLayoutManager());
  DCHECK(layout);
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(row.first);
  layout->AddView(row.second);

  auto* scroll_view = views::ScrollView::GetScrollViewForContents(this);
  DCHECK(scroll_view);
  if (!scroll_view->is_bounded()) {
    scroll_view->ClipHeightTo(
        0, std::max(row.first->GetPreferredSize().height(),
                    row.second->GetPreferredSize().height()) *
               kMaxVisibleListItems);
  }
}

// ContentSettingBubbleContents -----------------------------------------------

ContentSettingBubbleContents::ContentSettingBubbleContents(
    ContentSettingBubbleModel* content_setting_bubble_model,
    content::WebContents* web_contents,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : content::WebContentsObserver(web_contents),
      BubbleDialogDelegateView(anchor_view, arrow),
      content_setting_bubble_model_(content_setting_bubble_model),
      list_item_container_(nullptr),
      custom_link_(nullptr),
      manage_button_(nullptr),
      manage_checkbox_(nullptr),
      learn_more_button_(nullptr) {
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::CONTENT_SETTING_CONTENTS);
}

ContentSettingBubbleContents::~ContentSettingBubbleContents() {
  // Must remove the children here so the comboboxes get destroyed before
  // their associated models.
  RemoveAllChildViews(true);
}

void ContentSettingBubbleContents::WindowClosing() {
  content_setting_bubble_model_->CommitChanges();
}

gfx::Size ContentSettingBubbleContents::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void ContentSettingBubbleContents::OnListItemAdded(
    const ContentSettingBubbleModel::ListItem& item) {
  DCHECK(list_item_container_);
  list_item_container_->AddItem(item);
  SizeToContents();
}

void ContentSettingBubbleContents::OnListItemRemovedAt(int index) {
  DCHECK(list_item_container_);
  list_item_container_->RemoveRowAtIndex(index);
  SizeToContents();
}

int ContentSettingBubbleContents::GetSelectedRadioOption() {
  for (RadioGroup::const_iterator i(radio_group_.begin());
       i != radio_group_.end(); ++i) {
    if ((*i)->checked())
      return i - radio_group_.begin();
  }
  NOTREACHED();
  return 0;
}

void ContentSettingBubbleContents::OnNativeThemeChanged(
    const ui::NativeTheme* theme) {
  views::BubbleDialogDelegateView::OnNativeThemeChanged(theme);
  if (learn_more_button_)
    StyleLearnMoreButton(theme);
}

base::string16 ContentSettingBubbleContents::GetWindowTitle() const {
  return content_setting_bubble_model_->bubble_content().title;
}

bool ContentSettingBubbleContents::ShouldShowCloseButton() const {
  return true;
}

void ContentSettingBubbleContents::Init() {
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  std::vector<LayoutRow> rows;

  const ContentSettingBubbleModel::BubbleContent& bubble_content =
      content_setting_bubble_model_->bubble_content();

  if (!bubble_content.message.empty()) {
    auto message_label = std::make_unique<views::Label>(
        bubble_content.message, views::style::CONTEXT_LABEL, STYLE_SECONDARY);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    rows.push_back({std::move(message_label), LayoutRowType::DEFAULT});
  }

  // Layout for the item list (blocked plugins and popups).
  if (!bubble_content.list_items.empty()) {
    list_item_container_ = new ListItemContainer(this);
    list_item_container_->SetBorder(
        views::CreateEmptyBorder(0, margins().left(), 0, margins().right()));
    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->SetContents(list_item_container_);
    rows.push_back({std::move(scroll_view), LayoutRowType::FULL_WIDTH});

    for (const ContentSettingBubbleModel::ListItem& list_item :
         bubble_content.list_items) {
      list_item_container_->AddItem(list_item);
    }
  }

  const ContentSettingBubbleModel::RadioGroup& radio_group =
      bubble_content.radio_group;
  if (!radio_group.radio_items.empty()) {
    for (auto i(radio_group.radio_items.begin());
         i != radio_group.radio_items.end(); ++i) {
      auto radio = std::make_unique<views::RadioButton>(*i, 0);
      radio->SetEnabled(bubble_content.radio_group_enabled);
      radio->SetMultiLine(true);
      radio_group_.push_back(radio.get());
      rows.push_back({std::move(radio), LayoutRowType::INDENTED});
    }
    DCHECK(!radio_group_.empty());
    // Now that the buttons have been added to the view hierarchy, it's safe
    // to call SetChecked() on them.
    radio_group_[radio_group.default_item]->SetChecked(true);
  }

  // Layout code for the media device menus.
  if (content_setting_bubble_model_->AsMediaStreamBubbleModel()) {
    rows.push_back(
        {std::make_unique<MediaMenuBlock>(this, bubble_content.media_menus),
         LayoutRowType::INDENTED});
  }

  for (auto i(bubble_content.domain_lists.begin());
       i != bubble_content.domain_lists.end(); ++i) {
    auto list_view =
        std::make_unique<ContentSettingDomainListView>(i->title, i->hosts);
    rows.push_back({std::move(list_view), LayoutRowType::DEFAULT});
  }

  if (!bubble_content.custom_link.empty()) {
    auto custom_link =
        std::make_unique<views::Link>(bubble_content.custom_link);
    custom_link->SetEnabled(bubble_content.custom_link_enabled);
    custom_link->SetMultiLine(true);
    custom_link->set_listener(this);
    custom_link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    custom_link_ = custom_link.get();
    rows.push_back({std::move(custom_link), LayoutRowType::DEFAULT});
  }

  if (bubble_content.manage_text_style ==
      ContentSettingBubbleModel::ManageTextStyle::kCheckbox) {
    auto manage_checkbox =
        std::make_unique<views::Checkbox>(bubble_content.manage_text, this);
    manage_checkbox_ = manage_checkbox.get();
    rows.push_back({std::move(manage_checkbox), LayoutRowType::DEFAULT});
  }

  // We have to apply the left and right margins manually, because rows using
  // LayoutRowType::FULL_WIDTH need to not have them applied to look correct.
  const int left_margin = margins().left();
  const int right_margin = margins().right();
  set_margins(gfx::Insets(margins().top(), 0, margins().bottom(), 0));

  for (LayoutRow& row : rows) {
    if (row.type != LayoutRowType::FULL_WIDTH) {
      const int row_left_margin =
          left_margin + (row.type == LayoutRowType::INDENTED
                             ? provider->GetDistanceMetric(
                                   DISTANCE_SUBSECTION_HORIZONTAL_INDENT)
                             : 0);
      row.view->SetBorder(
          views::CreateEmptyBorder(0, row_left_margin, 0, right_margin));
    }
    AddChildView(row.view.release());
  }

  content_setting_bubble_model_->set_owner(this);
}

views::View* ContentSettingBubbleContents::CreateExtraView() {
  const auto& bubble_content = content_setting_bubble_model_->bubble_content();
  const auto* layout = ChromeLayoutProvider::Get();
  std::vector<View*> extra_views;
  // Optionally add a help icon if the view wants to link to a help page.
  if (bubble_content.show_learn_more) {
    learn_more_button_ = views::CreateVectorImageButton(this);
    learn_more_button_->SetFocusForPlatform();
    learn_more_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    StyleLearnMoreButton(GetNativeTheme());
    extra_views.push_back(learn_more_button_);
  }
  // Optionally add a "Manage" button if the view wants to use a button to
  // invoke a separate management UI related to the dialog content.
  if (bubble_content.manage_text_style ==
      ContentSettingBubbleModel::ManageTextStyle::kButton) {
    base::string16 title = bubble_content.manage_text;
    if (title.empty())
      title = l10n_util::GetStringUTF16(IDS_MANAGE);
    manage_button_ = views::MdTextButton::CreateSecondaryUiButton(this, title);
    manage_button_->SetMinSize(gfx::Size(
        layout->GetDistanceMetric(views::DISTANCE_DIALOG_BUTTON_MINIMUM_WIDTH),
        0));
    extra_views.push_back(manage_button_);
  }
  if (extra_views.empty())
    return nullptr;
  if (extra_views.size() == 1)
    return extra_views.front();
  views::View* container = new views::View();
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      layout->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  for (auto* extra_view : extra_views)
    container->AddChildView(extra_view);
  return container;
}

bool ContentSettingBubbleContents::Accept() {
  return true;
}

bool ContentSettingBubbleContents::Close() {
  return true;
}

int ContentSettingBubbleContents::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 ContentSettingBubbleContents::GetDialogButtonLabel(
    ui::DialogButton button) const {
  const base::string16& done_text =
      content_setting_bubble_model_->bubble_content().done_button_text;
  return done_text.empty() ? l10n_util::GetStringUTF16(IDS_DONE) : done_text;
}

void ContentSettingBubbleContents::StyleLearnMoreButton(
    const ui::NativeTheme* theme) {
  DCHECK(learn_more_button_);
  SkColor text_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);
  views::SetImageFromVectorIcon(learn_more_button_,
                                vector_icons::kHelpOutlineIcon, text_color);
}

void ContentSettingBubbleContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

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
  GetWidget()->Close();
}

void ContentSettingBubbleContents::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == manage_checkbox_) {
    content_setting_bubble_model_->OnManageCheckboxChecked(
        manage_checkbox_->checked());

    // Toggling the check state may change the dialog button text.
    DialogModelChanged();
    GetDialogClientView()->Layout();
  } else if (sender == learn_more_button_) {
    GetWidget()->Close();
    content_setting_bubble_model_->OnLearnMoreClicked();
  } else if (sender == manage_button_) {
    GetWidget()->Close();
    content_setting_bubble_model_->OnManageButtonClicked();
  } else {
    NOTREACHED();
  }
}

void ContentSettingBubbleContents::LinkClicked(views::Link* source,
                                               int event_flags) {
  if (source == custom_link_) {
    content_setting_bubble_model_->OnCustomLinkClicked();
    GetWidget()->Close();
    return;
  }
  int row = list_item_container_->GetRowIndexOf(source);
  DCHECK_NE(row, -1);
  content_setting_bubble_model_->OnListItemClicked(row, event_flags);
}

void ContentSettingBubbleContents::OnPerformAction(views::Combobox* combobox) {
  MediaComboboxModel* model =
      static_cast<MediaComboboxModel*>(combobox->model());
  content_setting_bubble_model_->OnMediaMenuClicked(
      model->type(), model->GetDevices()[combobox->selected_index()].id);
}
