// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils.h"

#include <stddef.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/resources/grit/ui_resources.h"
#endif

namespace chrome {
namespace {

using ::bookmarks::BookmarkModel;
using ::bookmarks::BookmarkNode;
using ::ui::mojom::DragOperation;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

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
#endif  // defined(TOOLKIT_VIEWS)

// Returns true if the `dragged_node` can be dropped on `drop_parent` at
// `index`. A drop is not allowed on a managed node. A drop from a separate
// profile or a URL is allowed, where as a drop from the same profile is only
// allowed if:
// - The `dragged_node` is not an ancestor of `drop_parent`
// - The `dragged _node` isn't already a child of `drop_parent` at `index`.
bool IsValidBookmarkDropLocation(
    const BookmarkMergedSurfaceService* bookmark_merged_service,
    const bookmarks::BookmarkNode* dragged_node,
    bool dragged_from_same_profile,
    const BookmarkParentFolder& drop_parent,
    size_t index) {
  if (bookmark_merged_service->IsParentFolderManaged(drop_parent)) {
    // Drop on a managed node is not allowed.
    return false;
  }

  if (!dragged_from_same_profile) {
    // If the drop is not on a managed node, always accept if node from another
    // profile or the user is dragging a URL.
    // Note: It is expected if the user is dragging a URL
    // `dragged_from_same_profile` is false as a URL is not associated with a
    // profile.
    return true;
  }

  // `dragged_node` is null if the node is from another profile or the user is
  // dragging a url. In both cases, `dragged_from_same_profile` is expected to
  // be false. For dragging a node within the same profile, the `dragged_node`
  // must be not null.
  CHECK(dragged_node);
  CHECK(!dragged_node->is_root());
  CHECK(!dragged_node->is_permanent_node());
  // Don't allow the drop if the user is attempting to drop on the node being
  // dragged.
  if (drop_parent.HasDirectChildNode(dragged_node)) {
    // Reordering.
    size_t node_index = bookmark_merged_service->GetIndexOf(dragged_node);
    if (index == node_index || index == node_index + 1) {
      return false;
    }
  }

  // drop_parent can't accept a child that is an ancestor.
  if (drop_parent.HoldsNonPermanentFolder() &&
      drop_parent.as_non_permanent_folder()->HasAncestor(dragged_node)) {
    return false;
  }

  return true;
}

}  // namespace

GURL GetURLToBookmark(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (search::IsInstantNTP(web_contents))
    return GURL(kChromeUINewTabURL);
  // Users cannot bookmark Reader Mode pages directly, so the bookmark
  // interaction is as if it were with the original page.
  if (dom_distiller::url_utils::IsDistilledPage(
          web_contents->GetVisibleURL())) {
    return dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
        web_contents->GetVisibleURL());
  }
  return web_contents->GetVisibleURL();
}

bool GetURLAndTitleToBookmark(content::WebContents* web_contents,
                              GURL* url,
                              std::u16string* title) {
  GURL u = GetURLToBookmark(web_contents);
  if (!u.is_valid())
    return false;
  *url = u;
  if (dom_distiller::url_utils::IsDistilledPage(
          web_contents->GetVisibleURL())) {
    // Users cannot bookmark Reader Mode pages directly. Instead, a bookmark
    // is added for the original page and original title.
    *title =
        base::UTF8ToUTF16(dom_distiller::url_utils::GetTitleFromDistillerUrl(
            web_contents->GetVisibleURL()));
  } else {
    *title = web_contents->GetTitle();
  }

  // Use "New tab" as title if the current page is NTP even in incognito mode.
  if (u == GURL(chrome::kChromeUINewTabURL))
    *title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);

  return true;
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

  return url_formatter::FormatUrl(url, format_types, base::UnescapeRule::SPACES,
                                  nullptr, nullptr, nullptr);
}

bool IsAppsShortcutEnabled(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
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

bool ShouldShowTabGroupsInBookmarkBar(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar);
}

int GetBookmarkDragOperation(content::BrowserContext* browser_context,
                             const BookmarkNode* node) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_context);

  int move = ui::DragDropTypes::DRAG_MOVE;
  if (!prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled) ||
      model->client()->IsNodeManaged(node)) {
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
                                       const BookmarkParentFolder& parent,
                                       size_t index) {
  if (data.size() != 1) {
    // Currently only accept one dragged element (bookmark node/url) at a time.
    return DragOperation::kNone;
  }

  BookmarkMergedSurfaceService* const bookmark_merged_service =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(profile);
  const BookmarkNode* const dragged_node = data.GetFirstNode(
      bookmark_merged_service->bookmark_model(), profile->GetPath());
  if (!IsValidBookmarkDropLocation(bookmark_merged_service, dragged_node,
                                   data.IsFromProfilePath(profile->GetPath()),
                                   parent, index)) {
    return DragOperation::kNone;
  }

  if (dragged_node) {
    // User is dragging from this profile.
    if (bookmark_merged_service->IsNodeManaged(dragged_node)) {
      // Do a copy instead of a move when dragging bookmarks that the user can't
      // modify.
      return DragOperation::kCopy;
    }
    return DragOperation::kMove;
  }

  // User is dragging from another app, copy.
  return GetPreferredBookmarkDropOperation(
      event.source_operations(),
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK);
}

bool CanAllBeEditedByUser(
    bookmarks::ManagedBookmarkService* managed_bookmark_service,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes) {
  if (!managed_bookmark_service) {
    return true;
  }

  for (const bookmarks::BookmarkNode* node : nodes) {
    if (managed_bookmark_service->IsNodeManaged(node)) {
      return false;
    }
  }
  return true;
}

#if defined(TOOLKIT_VIEWS)

gfx::ImageSkia GetBookmarkFolderImageFromVectorIcon(
    BookmarkFolderIconType icon_type,
    absl::variant<ui::ColorId, SkColor> color,
    const ui::ColorProvider* color_provider) {
  const gfx::VectorIcon* id;
  gfx::ImageSkia folder;
  if (icon_type == BookmarkFolderIconType::kNormal) {
    id = &vector_icons::kFolderChromeRefreshIcon;
  } else {
    id = &vector_icons::kFolderManagedRefreshIcon;
  }
  const ui::ThemedVectorIcon icon =
      absl::holds_alternative<SkColor>(color)
          ? ui::ThemedVectorIcon(id, absl::get<SkColor>(color))
          : ui::ThemedVectorIcon(id, absl::get<ui::ColorId>(color));
  folder = icon.GetImageSkia(color_provider);
  return folder;
}

ui::ImageModel GetBookmarkFolderIcon(
    BookmarkFolderIconType icon_type,
    absl::variant<ui::ColorId, SkColor> color) {
  int default_id = IDR_FOLDER_CLOSED;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // This block must be #ifdefed because only these platforms actually have this
  // resource ID.
  if (icon_type == BookmarkFolderIconType::kManaged) {
    default_id = IDR_BOOKMARK_BAR_FOLDER_MANAGED;
  }
#endif
  const auto generator = [](int default_id, BookmarkFolderIconType icon_type,
                            absl::variant<ui::ColorId, SkColor> color,
                            const ui::ColorProvider* color_provider) {
    gfx::ImageSkia folder;
    folder =
        GetBookmarkFolderImageFromVectorIcon(icon_type, color, color_provider);
    return gfx::ImageSkia(std::make_unique<RTLFlipSource>(folder),
                          folder.size());
  };
  const gfx::Size size =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(default_id).Size();
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(generator, default_id, icon_type, std::move(color)),
      size);
}
#endif

}  // namespace chrome
