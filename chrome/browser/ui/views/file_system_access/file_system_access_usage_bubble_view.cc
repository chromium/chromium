// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_views_helpers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
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
    const FileSystemAccessUsageBubbleView::Usage& usage,
    base::FilePath* embedded_path) {
  // Only files.
  if (usage.writable_directories.empty() &&
      usage.readable_directories.empty()) {
    // Only writable files.
    if (usage.readable_files.empty()) {
      DCHECK(!usage.writable_files.empty());
      if (usage.writable_files.size() == 1) {
        *embedded_path = usage.writable_files.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_WRITABLE_FILE_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_WRITABLE_FILES_TEXT;
    }

    // Only readable files.
    if (usage.writable_files.empty()) {
      DCHECK(!usage.readable_files.empty());
      if (usage.readable_files.size() == 1) {
        *embedded_path = usage.readable_files.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_READABLE_FILE_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READABLE_FILES_TEXT;
    }
  }

  // Only directories.
  if (usage.writable_files.empty() && usage.readable_files.empty()) {
    // Only writable directories.
    if (usage.readable_directories.empty()) {
      DCHECK(!usage.writable_directories.empty());
      if (usage.writable_directories.size() == 1) {
        *embedded_path = usage.writable_directories.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_WRITABLE_DIRECTORY_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_WRITABLE_DIRECTORIES_TEXT;
    }

    // Only readable directories.
    if (usage.writable_directories.empty()) {
      DCHECK(!usage.readable_directories.empty());
      if (usage.readable_directories.size() == 1) {
        *embedded_path = usage.readable_directories.front();
        return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SINGLE_READABLE_DIRECTORY_TEXT;
      }
      return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READABLE_DIRECTORIES_TEXT;
    }
  }

  // Only readable files and directories.
  if (usage.writable_files.empty() && usage.writable_directories.empty()) {
    DCHECK(!usage.readable_files.empty());
    DCHECK(!usage.readable_directories.empty());
    return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READABLE_FILES_AND_DIRECTORIES_TEXT;
  }

  // Only writable files and directories.
  if (usage.readable_files.empty() && usage.readable_directories.empty()) {
    DCHECK(!usage.writable_files.empty());
    DCHECK(!usage.writable_directories.empty());
    return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_WRITABLE_FILES_AND_DIRECTORIES_TEXT;
  }

  // Some combination of read and/or write access to files and/or directories.
  return IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_READ_AND_WRITE;
}

// Displays a (one-column) table model as a one-line summary showing the
// first few items, with a toggle button to expand a table below to contain the
// full list of items.
class CollapsibleListView : public views::View {
  METADATA_HEADER(CollapsibleListView, views::View)

 public:
  // How many rows to show in the expanded table without having to scroll.
  static constexpr int kExpandedTableRowCount = 3;

  explicit CollapsibleListView(ui::TableModel* model) {
    const views::LayoutProvider* provider = ChromeLayoutProvider::Get();

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(0),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

    auto label_container = std::make_unique<views::View>();
    int indent =
        provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
    auto* label_layout =
        label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::VH(0, indent),
            provider->GetDistanceMetric(
                views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
    std::u16string label_text;
    if (model->RowCount() > 0) {
      auto icon = std::make_unique<views::ImageView>();
      icon->SetImage(model->GetIcon(0));
      label_container->AddChildView(std::move(icon));

      std::u16string first_item = model->GetText(0, 0);
      std::u16string second_item =
          model->RowCount() > 1 ? model->GetText(1, 0) : std::u16string();

      label_text = base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_FILES_TEXT),
          base::checked_cast<int64_t>(model->RowCount()), first_item,
          second_item);
    }
    auto* label = label_container->AddChildView(std::make_unique<views::Label>(
        label_text, CONTEXT_DIALOG_BODY_TEXT_SMALL,
        views::style::STYLE_PRIMARY));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_layout->SetFlexForView(label, 1);
    auto button = views::CreateVectorToggleImageButton(base::BindRepeating(
        &CollapsibleListView::ButtonPressed, base::Unretained(this)));
    button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_USAGE_EXPAND));
    button->SetToggledTooltipText(
        l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_USAGE_COLLAPSE));
    expand_collapse_button_ = label_container->AddChildView(std::move(button));
    if (model->RowCount() < 3)
      expand_collapse_button_->SetVisible(false);
    int preferred_width = label_container->GetPreferredSize().width();
    AddChildView(std::move(label_container));

    std::vector<ui::TableColumn> table_columns{ui::TableColumn()};
    auto table_view = std::make_unique<views::TableView>(
        model, std::move(table_columns), views::TableType::kIconAndText,
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

  void ClearModel() {
    static_cast<views::TableView*>(table_view_parent_->contents())
        ->SetModel(nullptr);
  }

  // views::View
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    const auto* color_provider = GetColorProvider();
    const SkColor icon_color = color_provider->GetColor(ui::kColorIcon);
    const SkColor disabled_icon_color =
        color_provider->GetColor(ui::kColorIconDisabled);
    views::SetImageFromVectorIconWithColor(
        expand_collapse_button_, vector_icons::kCaretDownIcon,
        ui::TableModel::kIconSize, icon_color, disabled_icon_color);
    views::SetToggledImageFromVectorIconWithColor(
        expand_collapse_button_, vector_icons::kCaretUpIcon,
        ui::TableModel::kIconSize, icon_color, disabled_icon_color);
  }

 private:
  void ButtonPressed() {
    table_is_expanded_ = !table_is_expanded_;
    expand_collapse_button_->SetToggled(table_is_expanded_);
    table_view_parent_->SetVisible(table_is_expanded_);
    PreferredSizeChanged();
  }

  bool table_is_expanded_ = false;
  raw_ptr<views::ScrollView> table_view_parent_;
  raw_ptr<views::ToggleImageButton> expand_collapse_button_;
};

BEGIN_METADATA(CollapsibleListView)
END_METADATA

}  // namespace

FileSystemAccessUsageBubbleView::Usage::Usage() = default;
FileSystemAccessUsageBubbleView::Usage::~Usage() = default;
FileSystemAccessUsageBubbleView::Usage::Usage(Usage&&) = default;
FileSystemAccessUsageBubbleView::Usage&
FileSystemAccessUsageBubbleView::Usage::operator=(Usage&&) = default;

FileSystemAccessUsageBubbleView::FilePathListModel::FilePathListModel(
    std::vector<base::FilePath> files,
    std::vector<base::FilePath> directories)
    : files_(std::move(files)), directories_(std::move(directories)) {}

FileSystemAccessUsageBubbleView::FilePathListModel::~FilePathListModel() =
    default;

size_t FileSystemAccessUsageBubbleView::FilePathListModel::RowCount() {
  return files_.size() + directories_.size();
}

std::u16string FileSystemAccessUsageBubbleView::FilePathListModel::GetText(
    size_t row,
    int column_id) {
  // Use the non-eliding version of GetPathForDisplay since these are files the
  // user has already granted the site access to.
  if (row < files_.size()) {
    return file_system_access_ui_helper::GetPathForDisplayAsParagraph(
        content::PathInfo(files_[row]));
  }
  return file_system_access_ui_helper::GetPathForDisplayAsParagraph(
      content::PathInfo(directories_[row - files_.size()]));
}

ui::ImageModel FileSystemAccessUsageBubbleView::FilePathListModel::GetIcon(
    size_t row) {
  return ui::ImageModel::FromVectorIcon(
      row < files_.size() ? vector_icons::kInsertDriveFileOutlineIcon
                          : vector_icons::kFolderOpenIcon,
      ui::kColorIcon, kIconSize);
}

std::u16string FileSystemAccessUsageBubbleView::FilePathListModel::GetTooltip(
    size_t row) {
  if (row < files_.size())
    return files_[row].LossyDisplayName();
  return directories_[row - files_.size()].LossyDisplayName();
}

void FileSystemAccessUsageBubbleView::FilePathListModel::SetObserver(
    ui::TableModelObserver*) {}

// static
FileSystemAccessUsageBubbleView* FileSystemAccessUsageBubbleView::bubble_ =
    nullptr;

// static
void FileSystemAccessUsageBubbleView::ShowBubble(
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage) {
  base::RecordAction(
      base::UserMetricsAction("NativeFileSystemAPI.OpenedBubble"));

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return;

  ToolbarButtonProvider* button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();

  // Writable files or directories are generally also readable, but we don't
  // want to display the same path twice. So filter out any writable paths from
  // the readable lists.
  std::set<base::FilePath> writable_directories(
      usage.writable_directories.begin(), usage.writable_directories.end());
  std::erase_if(usage.readable_directories, [&](const base::FilePath& path) {
    return base::Contains(writable_directories, path);
  });
  std::set<base::FilePath> writable_files(usage.writable_files.begin(),
                                          usage.writable_files.end());
  std::erase_if(usage.readable_files, [&](const base::FilePath& path) {
    return base::Contains(writable_files, path);
  });

  bubble_ = new FileSystemAccessUsageBubbleView(
      button_provider->GetAnchorView(PageActionIconType::kFileSystemAccess),
      web_contents, origin, std::move(usage));

  bubble_->SetHighlightedButton(button_provider->GetPageActionIconView(
      PageActionIconType::kFileSystemAccess));
  views::BubbleDialogDelegateView::CreateBubble(bubble_);

  bubble_->ShowForReason(DisplayReason::USER_GESTURE,
                         /*allow_refocus_alert=*/true);
}

// static
void FileSystemAccessUsageBubbleView::CloseCurrentBubble() {
  if (bubble_)
    bubble_->CloseBubble();
}

// static
FileSystemAccessUsageBubbleView* FileSystemAccessUsageBubbleView::GetBubble() {
  return bubble_;
}

FileSystemAccessUsageBubbleView::FileSystemAccessUsageBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    const url::Origin& origin,
    Usage usage)
    : LocationBarBubbleDelegateView(anchor_view,
                                    web_contents,
                                    /*autosize=*/true),
      origin_(origin),
      usage_(std::move(usage)),
      readable_paths_model_(std::move(usage_.readable_files),
                            std::move(usage_.readable_directories)),
      writable_paths_model_(std::move(usage_.writable_files),
                            std::move(usage_.writable_directories)) {
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_DONE));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_ACCESS_USAGE_REMOVE_ACCESS));
  SetCancelCallback(
      base::BindOnce(&FileSystemAccessUsageBubbleView::OnDialogCancelled,
                     base::Unretained(this)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

FileSystemAccessUsageBubbleView::~FileSystemAccessUsageBubbleView() {
  if (readable_collapsible_list_view_) {
    static_cast<CollapsibleListView*>(readable_collapsible_list_view_)
        ->ClearModel();
  }
  if (writable_collapsible_list_view_) {
    static_cast<CollapsibleListView*>(writable_collapsible_list_view_)
        ->ClearModel();
  }
}

std::u16string FileSystemAccessUsageBubbleView::GetAccessibleWindowTitle()
    const {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  // Don't crash if the web_contents is destroyed/unloaded.
  if (!browser)
    return {};

  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetPageActionIconView(PageActionIconType::kFileSystemAccess)
      ->GetTextForTooltipAndAccessibleName();
}

bool FileSystemAccessUsageBubbleView::ShouldShowCloseButton() const {
  return true;
}

void FileSystemAccessUsageBubbleView::Init() {
  // Set up the layout of the bubble.
  const views::LayoutProvider* provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets =
      provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(0, dialog_insets.left(), 0, dialog_insets.right()),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(gfx::Insets::TLBR(
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_TEXT),
      0,
      provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
      0));

  base::FilePath embedded_path;
  int heading_message_id =
      ComputeHeadingMessageFromUsage(usage_, &embedded_path);

  if (!embedded_path.empty()) {
    AddChildView(file_system_access_ui_helper::CreateOriginPathLabel(
        web_contents(), heading_message_id, origin_, embedded_path,
        views::style::CONTEXT_DIALOG_BODY_TEXT,
        /*show_emphasis=*/false));
  } else {
    AddChildView(file_system_access_ui_helper::CreateOriginLabel(
        web_contents(), heading_message_id, origin_,
        views::style::CONTEXT_DIALOG_BODY_TEXT,
        /*show_emphasis=*/false));

    if (writable_paths_model_.RowCount() > 0) {
      if (readable_paths_model_.RowCount() > 0) {
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_SAVE_CHANGES),
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_PRIMARY);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(std::move(label));
      }
      writable_collapsible_list_view_ = AddChildView(
          std::make_unique<CollapsibleListView>(&writable_paths_model_));
    }

    if (readable_paths_model_.RowCount() > 0) {
      if (writable_paths_model_.RowCount() > 0) {
        auto label = std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_FILE_SYSTEM_ACCESS_USAGE_BUBBLE_VIEW_CHANGES),
            views::style::CONTEXT_DIALOG_BODY_TEXT,
            views::style::STYLE_PRIMARY);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        AddChildView(std::move(label));
      }
      readable_collapsible_list_view_ = AddChildView(
          std::make_unique<CollapsibleListView>(&readable_paths_model_));
    }
  }
}

void FileSystemAccessUsageBubbleView::OnDialogCancelled() {
  base::RecordAction(
      base::UserMetricsAction("NativeFileSystemAPI.RevokePermissions"));

  if (!web_contents())
    return;

  content::BrowserContext* profile = web_contents()->GetBrowserContext();
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(profile);
  if (!context)
    return;

  context->RevokeGrants(origin_);
}

void FileSystemAccessUsageBubbleView::WindowClosing() {
  // |bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to nullptr when it's this bubble.
  if (bubble_ == this)
    bubble_ = nullptr;
}

void FileSystemAccessUsageBubbleView::CloseBubble() {
  // Widget's Close() is async, but we don't want to use bubble_ after
  // this. Additionally web_contents() may have been destroyed.
  bubble_ = nullptr;
  LocationBarBubbleDelegateView::CloseBubble();
}

BEGIN_METADATA(FileSystemAccessUsageBubbleView)
END_METADATA
