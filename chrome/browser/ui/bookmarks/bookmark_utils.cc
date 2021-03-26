// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils.h"

#include <stddef.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/search/search.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/pointer/touch_ui_controller.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#endif

namespace chrome {
namespace {

using ::bookmarks::BookmarkModel;
using ::bookmarks::BookmarkNode;
using ::ui::mojom::DragOperation;

#if defined(TOOLKIT_VIEWS)
// Image source that flips the supplied source image in RTL.
class RTLFlipSource : public gfx::ImageSkiaSource {
 public:
  explicit RTLFlipSource(gfx::ImageSkia source) : source_(std::move(source)) {}
  ~RTLFlipSource() override = default;

  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    gfx::Canvas canvas(source_.size(), scale, false);
    gfx::ScopedCanvas scoped_canvas(&canvas);
    scoped_canvas.FlipIfRTL(source_.width());
    canvas.DrawImageInt(source_, 0, 0);
    return gfx::ImageSkiaRep(canvas.GetBitmap(), scale);
  }

 private:
  const gfx::ImageSkia source_;
};

#if !defined(OS_WIN) && !defined(OS_MAC)
gfx::ImageSkia GetFolderIcon(const gfx::VectorIcon& icon, SkColor text_color) {
  return gfx::CreateVectorIcon(icon,
                               color_utils::DeriveDefaultIconColor(text_color));
}
#endif  // !defined(OS_WIN) && !defined(OS_MAC)
#endif  // defined(TOOLKIT_VIEWS)

}  // namespace

GURL GetURLToBookmark(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (search::IsInstantNTP(web_contents))
    return GURL(kChromeUINewTabURL);
  // Users cannot bookmark Reader Mode pages directly, so the bookmark
  // interaction is as if it were with the original page.
  if (dom_distiller::url_utils::IsDistilledPage(web_contents->GetURL())) {
    return dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
        web_contents->GetURL());
  }
  return web_contents->GetURL();
}

void GetURLAndTitleToBookmark(content::WebContents* web_contents,
                              GURL* url,
                              std::u16string* title) {
  *url = GetURLToBookmark(web_contents);
  if (dom_distiller::url_utils::IsDistilledPage(web_contents->GetURL())) {
    // Users cannot bookmark Reader Mode pages directly. Instead, a bookmark
    // is added for the original page and original title.
    *title =
        base::UTF8ToUTF16(dom_distiller::url_utils::GetTitleFromDistillerUrl(
            web_contents->GetURL()));
  } else {
    *title = web_contents->GetTitle();
  }
}

void ToggleBookmarkBarWhenVisible(content::BrowserContext* browser_context) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  const bool always_show =
      !prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar);

  // The user changed when the bookmark bar is shown, update the preferences.
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, always_show);
}

std::u16string FormatBookmarkURLForDisplay(const GURL& url) {
  // Because this gets re-parsed by FixupURL(), it's safe to omit the scheme
  // and trailing slash, and unescape most characters. However, it's
  // important not to drop any username/password, or unescape anything that
  // changes the URL's meaning.
  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults &
      ~url_formatter::kFormatUrlOmitUsernamePassword;

  // If username is present, we must not omit the scheme because FixupURL() will
  // subsequently interpret the username as a scheme. crbug.com/639126
  if (url.has_username())
    format_types &= ~url_formatter::kFormatUrlOmitHTTP;

  return url_formatter::FormatUrl(url, format_types, net::UnescapeRule::SPACES,
                                  nullptr, nullptr, nullptr);
}

bool IsAppsShortcutEnabled(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS uses the app list / app launcher.
  return false;
#else
  return search::IsInstantExtendedAPIEnabled() && !profile->IsOffTheRecord();
#endif
}

bool ShouldShowAppsShortcutInBookmarkBar(Profile* profile) {
  return IsAppsShortcutEnabled(profile) &&
         profile->GetPrefs()->GetBoolean(
             bookmarks::prefs::kShowAppsShortcutInBookmarkBar);
}

bool ShouldShowReadingListInBookmarkBar(Profile* profile) {
  return base::FeatureList::IsEnabled(reading_list::switches::kReadLater) &&
         profile->GetPrefs()->GetBoolean(
             bookmarks::prefs::kShowReadingListInBookmarkBar);
}

int GetBookmarkDragOperation(content::BrowserContext* browser_context,
                             const BookmarkNode* node) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_context);

  int move = ui::DragDropTypes::DRAG_MOVE;
  if (!prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) ||
      !model->client()->CanBeEditedByUser(node)) {
    move = ui::DragDropTypes::DRAG_NONE;
  }
  if (node->is_url())
    return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK | move;
  return ui::DragDropTypes::DRAG_COPY | move;
}

DragOperation GetPreferredBookmarkDropOperation(int source_operations,
                                                int operations) {
  int common_ops = (source_operations & operations);
  if (!common_ops)
    return DragOperation::kNone;
  if (ui::DragDropTypes::DRAG_COPY & common_ops)
    return DragOperation::kCopy;
  if (ui::DragDropTypes::DRAG_LINK & common_ops)
    return DragOperation::kLink;
  if (ui::DragDropTypes::DRAG_MOVE & common_ops)
    return DragOperation::kMove;
  return DragOperation::kNone;
}

DragOperation GetBookmarkDropOperation(Profile* profile,
                                       const ui::DropTargetEvent& event,
                                       const bookmarks::BookmarkNodeData& data,
                                       const BookmarkNode* parent,
                                       size_t index) {
  const base::FilePath& profile_path = profile->GetPath();

  if (data.IsFromProfilePath(profile_path) && data.size() > 1)
    // Currently only accept one dragged node at a time.
    return DragOperation::kNone;

  if (!IsValidBookmarkDropLocation(profile, data, parent, index))
    return DragOperation::kNone;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (!model->client()->CanBeEditedByUser(parent))
    return DragOperation::kNone;

  const BookmarkNode* dragged_node =
      data.GetFirstNode(model, profile->GetPath());
  if (dragged_node) {
    // User is dragging from this profile.
    if (!model->client()->CanBeEditedByUser(dragged_node)) {
      // Do a copy instead of a move when dragging bookmarks that the user can't
      // modify.
      return DragOperation::kCopy;
    }
    return DragOperation::kMove;
  }

  // User is dragging from another app, copy.
  return GetPreferredBookmarkDropOperation(event.source_operations(),
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK);
}

bool IsValidBookmarkDropLocation(Profile* profile,
                                 const bookmarks::BookmarkNodeData& data,
                                 const BookmarkNode* drop_parent,
                                 size_t index) {
  if (!drop_parent->is_folder()) {
    NOTREACHED();
    return false;
  }

  if (!data.is_valid())
    return false;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  if (!model->client()->CanBeEditedByUser(drop_parent))
    return false;

  const base::FilePath& profile_path = profile->GetPath();
  if (data.IsFromProfilePath(profile_path)) {
    std::vector<const BookmarkNode*> nodes = data.GetNodes(model, profile_path);
    for (size_t i = 0; i < nodes.size(); ++i) {
      // Don't allow the drop if the user is attempting to drop on one of the
      // nodes being dragged.
      const BookmarkNode* node = nodes[i];
      int node_index = (drop_parent == node->parent()) ?
          drop_parent->GetIndexOf(nodes[i]) : -1;
      if (node_index != -1 &&
          (index == size_t{node_index} || index == size_t{node_index} + 1))
        return false;

      // drop_parent can't accept a child that is an ancestor.
      if (drop_parent->HasAncestor(node))
        return false;
    }
    return true;
  }
  // From another profile, always accept.
  return true;
}

#if defined(TOOLKIT_VIEWS)
// TODO(bsep): vectorize the Windows versions: crbug.com/564112
ui::ImageModel GetBookmarkFolderIcon(SkColor text_color) {
  gfx::ImageSkia folder;
#if defined(OS_WIN)
  folder = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_FOLDER_CLOSED);
#elif defined(OS_MAC)
  int resource_id = color_utils::IsDark(text_color) ? IDR_FOLDER_CLOSED
                                                    : IDR_FOLDER_CLOSED_WHITE;
  folder = *ui::ResourceBundle::GetSharedInstance()
                .GetNativeImageNamed(resource_id)
                .ToImageSkia();
#else
  folder = GetFolderIcon(ui::TouchUiController::Get()->touch_ui()
                             ? vector_icons::kFolderTouchIcon
                             : vector_icons::kFolderIcon,
                         text_color);
#endif
  // TODO(crbug.com/1119823): Return the unflipped image here
  // (as a vector if possible); callers should have the responsibility to flip
  // when painting as necessary.
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia(std::make_unique<RTLFlipSource>(folder), folder.size()));
}

ui::ImageModel GetBookmarkManagedFolderIcon(SkColor text_color) {
  gfx::ImageSkia folder;
#if defined(OS_WIN)
  folder = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_BOOKMARK_BAR_FOLDER_MANAGED);
#elif defined(OS_MAC)
  int resource_id = color_utils::IsDark(text_color)
                        ? IDR_BOOKMARK_BAR_FOLDER_MANAGED
                        : IDR_BOOKMARK_BAR_FOLDER_MANAGED_WHITE;
  folder = *ui::ResourceBundle::GetSharedInstance()
                .GetNativeImageNamed(resource_id)
                .ToImageSkia();
#else
  folder = GetFolderIcon(ui::TouchUiController::Get()->touch_ui()
                             ? vector_icons::kFolderManagedTouchIcon
                             : vector_icons::kFolderManagedIcon,
                         text_color);
#endif
  // TODO(crbug.com/1119823): Return the unflipped image here
  // (as a vector if possible); callers should have the responsibility to flip
  // when painting as necessary.
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia(std::make_unique<RTLFlipSource>(folder), folder.size()));
}
#endif

}  // namespace chrome
