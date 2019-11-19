// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/native_file_system/native_file_system_usage_bubble_view.h"

#include "base/i18n/message_formatter.h"
#include "base/i18n/unicodestring.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/native_file_system/native_file_system_ui_helpers.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Returns the message Id to use as heading text, depending on what types of
// usage are present (i.e. just writable files, or also readable directories,
// etc).
// |need_lifetime_text_at_end| is set to false iff the returned message Id
// already includes an explanation for how long a website will have access to
// the listed paths. It is set to true iff a separate label is needed at the end
// of the dialog to explain lifetime.
int ComputeHeadingMessageFromUsage(
    const NativeFileSystemUsageBubbleView::Usage& usage,
    base::FilePath* embedded_path) {
  // Only writable files.
  if (usage.writable_directories.empty() &&
      usage.readable_directories.empty()) {
    if (usage.writable_files.size() == 1) {
      *embedded_path = usage.writable_files.front();
      return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_SINGLE_WRITABLE_FILE_TEXT;
    }
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_FILES_TEXT;
  }

  // Only writable directories.
  if (usage.writable_files.empty() && usage.readable_directories.empty()) {
    if (usage.writable_directories.size() == 1) {
      *embedded_path = usage.writable_directories.front();
      return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_SINGLE_WRITABLE_DIRECTORY_TEXT;
    }
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_DIRECTORIES_TEXT;
  }

  // Both writable files and writable directories, but no read-only directories.
  if (usage.readable_directories.empty()) {
    DCHECK(!usage.writable_files.empty());
    DCHECK(!usage.writable_directories.empty());
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_WRITABLE_FILES_AND_DIRECTORIES_TEXT;
  }

  // Only readable directories.
  if (usage.writable_files.empty() && usage.writable_directories.empty()) {
    if (usage.readable_directories.size() == 1) {
      *embedded_path = usage.readable_directories.front();
      return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_SINGLE_READABLE_DIRECTORY_TEXT;
    }
    return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_READABLE_DIRECTORIES_TEXT;
  }

  // Some combination of read and write access.
  return IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_READ_AND_WRITE;
}

// Displays a (one-column) table model as a one-line summary showing the
// first few items, with a toggle button to expand a table below to contain the
// full list of items.
class CollapsibleListView : public views::View, public views::ButtonListener {
 public:
  // How many rows to show in the expanded table without having to scroll.
  static constexpr int kExpandedTableRowCount = 3;

  explicit CollapsibleListView(ui::TableModel* model) {
    const SkColor icon_color =
        ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
            ui::NativeTheme::kColorId_DefaultIconColor);
    const views::LayoutProvider* provider = ChromeLayoutProvider::Get();

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(0, 0),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    auto label_container = std::make_unique<views::View>();
    int indent =
        provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
    auto* label_layout =
        label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(/*vertical=*/0, indent),
            provider->GetDistanceMetric(
                views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    base::string16 label_text;
    if (model->RowCount() > 0) {
      auto icon = std::make_unique<views::ImageView>();
      icon->SetImage(model->GetIcon(0));
      label_container->AddChildView(std::move(icon));

      base::string16 first_item = model->GetText(0, 0);
      base::string16 second_item =
          model->RowCount() > 1 ? model->GetText(1, 0) : base::string16();

      label_text = base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_FILES_TEXT),
          model->RowCount(), first_item, second_item);
    }
    auto* label = label_container->AddChildView(std::make_unique<views::Label>(
        label_text, CONTEXT_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_layout->SetFlexForView(label, 1);
    auto button = views::CreateVectorToggleImageButton(this);
    views::SetImageFromVectorIconWithColor(
        button.get(), kCaretDownIcon, ui::TableModel::kIconSize, icon_color);
    button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_NATIVE_FILE_SYSTEM_USAGE_EXPAND));
    views::SetToggledImageFromVectorIconWithColor(
        button.get(), kCaretUpIcon, ui::TableModel::kIconSize, icon_color);
    button->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_NATIVE_FILE_SYSTEM_USAGE_COLLAPSE));
    expand_collapse_button_ = label_container->AddChildView(std::move(button));
    if (model->RowCount() < 3)
      expand_collapse_button_->SetVisible(false);
    int preferred_width = label_container->GetPreferredSize().width();
    AddChildView(std::move(label_container));

    std::vector<ui::TableColumn> table_columns{ui::TableColumn()};
    auto table_view = std::make_unique<views::TableView>(
        model, std::move(table_columns), views::ICON_AND_TEXT,
        /*single_selection=*/true);
    table_view->SetEnabled(false);
    int row_height = table_view->GetRowHeight();
    int table_height = table_view->GetPreferredSize().height();
    table_view_parent_ = AddChildView(
        views::TableView::CreateScrollViewWithTable(std::move(table_view)));
    // Ideally we'd use table_view_parent_->GetInsets().height(), but that only
    // returns the correct value after the view has been added to a root widget.
    // So just hardcode the inset height to 2 pixels as that is what the scroll
    // view uses.
    int inset_height = 2;
    table_view_parent_->SetPreferredSize(
        gfx::Size(preferred_width,
                  std::min(table_height, kExpandedTableRowCount * row_height) +
                      inset_height));
    table_view_parent_->SetVisible(false);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    table_is_expanded_ = !table_is_expanded_;
    expand_collapse_button_->SetToggled(table_is_expanded_);
    table_view_parent_->SetVisible(table_is_expanded_);
    PreferredSizeChanged();
  }

 private:
  bool table_is_expanded_ = false;
  views::ScrollView* table_view_parent_;
  views::ToggleImageButton* expand_collapse_button_;
};

}  // namespace

NativeFileSystemUsageBubbleView::Usage::Usage() = default;
NativeFileSystemUsageBubbleView::Usage::~Usage() = default;
NativeFileSystemUsageBubbleView::Usage::Usage(Usage&&) = default;
NativeFileSystemUsageBubbleView::Usage& NativeFileSystemUsageBubbleView::Usage::
operator=(Usage&&) = default;

NativeFileSystemUsageBubbleView::FilePathListModel::FilePathListModel(
    std::vector<base::FilePath> files,
    std::vector<base::FilePath> directories)
    : files_(std::move(files)), directories_(std::move(directories)) {}

NativeFileSystemUsageBubbleView::FilePathListModel::~FilePathListModel() =
    default;

int NativeFileSystemUsageBubbleView::FilePathListModel::RowCount() {
  return files_.size() + directories_.size();
}

base::string16 NativeFileSystemUsageBubbleView::FilePathListModel::GetText(
    int row,
    int column_id) {
  if (size_t{row} < files_.size())
    return files_[row].BaseName().LossyDisplayName();
  return directories_[row - files_.size()].BaseName().LossyDisplayName();
}

gfx::ImageSkia NativeFileSystemUsageBubbleView::FilePathListModel::GetIcon(
    int row) {
  return gfx::CreateVectorIcon(size_t{row} < files_.size()
                                   ? vector_icons::kInsertDriveFileOutlineIcon
                                   : vector_icons::kFolderOpenIcon,
                               kIconSize, gfx::kChromeIconGrey);
}

base::string16 NativeFileSystemUsageBubbleView::FilePathListModel::GetTooltip(
    int row) {
  if (size_t{row} < files_.size())
    return files_[row].LossyDisplayName();
  return directories_[row - files_.size()].LossyDisplayName();
}

void NativeFileSystemUsageBubbleView::FilePathListModel::SetObserver(
    ui::TableModelObserver*) {}

// static
NativeFileSystemUsageBubbleView* NativeFileSystemUsageBubbleView::bubble_ =
    nullptr;

// static
void NativeFileSystemUsageBubbleView::ShowBubble(
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage) {
  base::RecordAction(
      base::UserMetricsAction("NativeFileSystemAPI.OpenedBubble"));

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();

  // Writable directories are generally also readable, but we don't want to
  // display the same directory twice. So filter out any writable directories
  // from the readable directories list.
  std::set<base::FilePath> writable_directories(
      usage.writable_directories.begin(), usage.writable_directories.end());
  std::vector<base::FilePath> readable_directories;
  for (base::FilePath& path : usage.readable_directories) {
    if (!base::Contains(writable_directories, path))
      readable_directories.push_back(std::move(path));
  }
  usage.readable_directories = readable_directories;

  bubble_ = new NativeFileSystemUsageBubbleView(
      button_provider->GetAnchorView(
          PageActionIconType::kNativeFileSystemAccess),
      web_contents, origin, std::move(usage));

  bubble_->SetHighlightedButton(button_provider->GetPageActionIconView(
      PageActionIconType::kNativeFileSystemAccess));
  views::BubbleDialogDelegateView::CreateBubble(bubble_);

  bubble_->ShowForReason(DisplayReason::USER_GESTURE,
                         /*allow_refocus_alert=*/true);
}

// static
void NativeFileSystemUsageBubbleView::CloseCurrentBubble() {
  if (bubble_)
    bubble_->CloseBubble();
}

// static
NativeFileSystemUsageBubbleView* NativeFileSystemUsageBubbleView::GetBubble() {
  return bubble_;
}

NativeFileSystemUsageBubbleView::NativeFileSystemUsageBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      origin_(origin),
      usage_(std::move(usage)),
      writable_paths_model_(usage_.writable_files, usage_.writable_directories),
      readable_paths_model_({}, usage_.readable_directories) {
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   l10n_util::GetStringUTF16(IDS_DONE));
}

NativeFileSystemUsageBubbleView::~NativeFileSystemUsageBubbleView() = default;

base::string16 NativeFileSystemUsageBubbleView::GetAccessibleWindowTitle()
    const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // Don't crash if the web_contents is destroyed/unloaded.
  if (!browser)
    return {};

  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetPageActionIconView(PageActionIconType::kNativeFileSystemAccess)
      ->GetTextForTooltipAndAccessibleName();
}

base::string16 NativeFileSystemUsageBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  int message_id = IDS_DONE;
  if (button == ui::DIALOG_BUTTON_CANCEL)
    message_id = IDS_NATIVE_FILE_SYSTEM_USAGE_REMOVE_ACCESS;
  return l10n_util::GetStringUTF16(message_id);
}

bool NativeFileSystemUsageBubbleView::ShouldShowCloseButton() const {
  return true;
}

void NativeFileSystemUsageBubbleView::Init() {
  // Set up the layout of the bubble.
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, dialog_insets.left(), 0, dialog_insets.right()),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(
      gfx::Insets(provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
                  0,
                  provider->GetDistanceMetric(
                      views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                  0));

  base::FilePath embedded_path;
  int heading_message_id =
      ComputeHeadingMessageFromUsage(usage_, &embedded_path);

  if (!embedded_path.empty()) {
    AddChildView(native_file_system_ui_helper::CreateOriginPathLabel(
        heading_message_id, origin_, embedded_path, CONTEXT_BODY_TEXT_LARGE,
        /*show_emphasis=*/false));
  } else {
    AddChildView(native_file_system_ui_helper::CreateOriginLabel(
        heading_message_id, origin_, CONTEXT_BODY_TEXT_LARGE,
        /*show_emphasis=*/false));

    if (writable_paths_model_.RowCount() > 0) {
      if (readable_paths_model_.RowCount() > 0) {
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_SAVE_CHANGES),
            CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_PRIMARY);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(std::move(label));
      }
      AddChildView(
          std::make_unique<CollapsibleListView>(&writable_paths_model_));
    }

    if (readable_paths_model_.RowCount() > 0) {
      if (writable_paths_model_.RowCount() > 0) {
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_NATIVE_FILE_SYSTEM_USAGE_BUBBLE_VIEW_CHANGES),
            CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_PRIMARY);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(std::move(label));
      }
      AddChildView(
          std::make_unique<CollapsibleListView>(&readable_paths_model_));
    }
  }
}

bool NativeFileSystemUsageBubbleView::Cancel() {
  base::RecordAction(
      base::UserMetricsAction("NativeFileSystemAPI.RevokePermissions"));

  if (!web_contents())
    return true;

  content::BrowserContext* profile = web_contents()->GetBrowserContext();
  auto* context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(profile);
  if (!context)
    return true;

  context->RevokeGrantsForOriginAndTab(
      origin_, web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID());
  return true;
}

bool NativeFileSystemUsageBubbleView::Close() {
  return true;  // Do not revoke permissions via Cancel() when closing normally.
}

void NativeFileSystemUsageBubbleView::WindowClosing() {
  // |bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  if (bubble_ == this)
    bubble_ = nullptr;
}

void NativeFileSystemUsageBubbleView::CloseBubble() {
  // Widget's Close() is async, but we don't want to use bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

gfx::Size NativeFileSystemUsageBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void NativeFileSystemUsageBubbleView::ChildPreferredSizeChanged(
    views::View* child) {
  LocationBarBubbleDelegateView::ChildPreferredSizeChanged(child);
  SizeToContents();
}
