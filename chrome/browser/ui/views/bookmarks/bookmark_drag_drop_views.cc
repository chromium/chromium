// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/grit/platform_locale_settings.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/render_text.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/drag_utils.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace chrome {

namespace {

class BookmarkDragHelper;

// Generates a bookmark drag and drop chip image.
class BookmarkDragImageSource : public gfx::CanvasImageSource {
 public:
  // These DIP measurements come from the Bookmarks Drag Drop spec.
  static constexpr int kContainerWidth = 172;
  static constexpr int kContainerHeight = 40;
  static constexpr int kContainerRadius = kContainerHeight / 2;
  static constexpr SkColor kContainerColor = gfx::kGoogleBlue500;

  static constexpr int kIconContainerRadius = 12;
  static constexpr int kIconSize = 16;
  static constexpr SkColor kIconContainerColor = SK_ColorWHITE;

  static constexpr int kTitlePadding = 12;

  static constexpr int kCountPadding = 5;
  static constexpr int kCountContainerRadius = 12;
  static constexpr SkColor kCountContainerColor = gfx::kGoogleRed500;

  static constexpr gfx::Size kBookmarkDragImageSize =
      gfx::Size(kContainerWidth, kContainerHeight + kCountContainerRadius);

  static constexpr int kDragImageOffsetX = kContainerWidth / 2;
  static constexpr int kDragImageOffsetY = 0.9 * kContainerHeight;

  BookmarkDragImageSource(const base::string16& title,
                          const gfx::ImageSkia& icon,
                          size_t count)
      : gfx::CanvasImageSource(kBookmarkDragImageSize),
        title_(title),
        icon_(icon),
        count_(count) {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags paint_flags;
    paint_flags.setAntiAlias(true);

    // Draw background.
    gfx::RectF container_rect(0, kCountContainerRadius, kContainerWidth,
                              kContainerHeight);
    paint_flags.setColor(kContainerColor);
    canvas->DrawRoundRect(container_rect, kContainerRadius, paint_flags);

    // Draw icon container.
    paint_flags.setColor(kIconContainerColor);
    canvas->DrawCircle(
        gfx::PointF(kContainerRadius, kContainerRadius + kCountContainerRadius),
        kIconContainerRadius, paint_flags);

    // Draw icon image.
    canvas->DrawImageInt(
        icon_, kContainerRadius - kIconSize / 2,
        kContainerRadius + kIconContainerRadius - kIconSize / 2);

    // Draw bookmark title.
    gfx::FontList font_list = views::style::GetFont(
        views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
    gfx::Rect text_rect(kBookmarkDragImageSize);
    text_rect.Inset(kContainerRadius + kIconContainerRadius + kTitlePadding,
                    kCountContainerRadius,
                    kContainerRadius - kIconContainerRadius, 0);
    canvas->DrawStringRectWithFlags(title_, font_list, SK_ColorWHITE, text_rect,
                                    gfx::Canvas::TEXT_ALIGN_LEFT);

    if (count_ <= 1)
      return;

    // Draw bookmark count if more than 1 bookmark is dragged.
    base::string16 count = base::NumberToString16(count_);
    auto render_text = gfx::RenderText::CreateFor(gfx::Typesetter::BROWSER);
    render_text->SetFontList(font_list);
    render_text->SetCursorEnabled(false);
    render_text->SetColor(SK_ColorWHITE);
    render_text->SetText(count);
    render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    // We measure the count text size to determine container width, as the
    // container is a rounded rect behind the text.
    int count_width = render_text->GetStringSize().width();
    int count_container_width =
        std::max(kCountContainerRadius * 2, count_width + 2 * kCountPadding);

    // Draw the count container.
    gfx::Rect count_container_rect(
        container_rect.right() - count_container_width, 0,
        count_container_width, kCountContainerRadius * 2);
    paint_flags.setColor(kCountContainerColor);
    canvas->DrawRoundRect(gfx::RectF(count_container_rect),
                          kCountContainerRadius, paint_flags);

    // Draw the count text.
    render_text->SetDisplayRect(count_container_rect);
    render_text->Draw(canvas);
  }

  const base::string16 title_;
  const gfx::ImageSkia icon_;
  const int count_;
};

constexpr gfx::Size BookmarkDragImageSource::kBookmarkDragImageSize;

// Helper class that takes a drag request, loads the icon from the bookmark
// model and then launches a system drag with a generated drag image.
// Owns itself.
class BookmarkDragHelper : public bookmarks::BaseBookmarkModelObserver {
 public:
  static base::WeakPtr<BookmarkDragHelper> Create(
      Profile* profile,
      const BookmarkDragParams& params,
      DoBookmarkDragCallback do_drag_callback) {
    base::WeakPtr<BookmarkDragHelper> ptr =
        (new BookmarkDragHelper(profile, params, std::move(do_drag_callback)))
            ->GetWeakPtr();
    ptr->Start(params.nodes.at(params.drag_node_index));
    return ptr;
  }

 private:
  BookmarkDragHelper(Profile* profile,
                     const BookmarkDragParams& params,
                     DoBookmarkDragCallback do_drag_callback)
      : model_(BookmarkModelFactory::GetForBrowserContext(profile)),
        count_(params.nodes.size()),
        native_view_(params.view),
        source_(params.source),
        start_point_(params.start_point),
        do_drag_callback_(std::move(do_drag_callback)),
        drag_data_(std::make_unique<ui::OSExchangeData>()),
        observer_(this) {
    observer_.Add(model_);

    // Set up our OLE machinery.
    bookmarks::BookmarkNodeData bookmark_drag_data(params.nodes);
    bookmark_drag_data.Write(profile->GetPath(), drag_data_.get());

    operation_ = ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
    if (bookmarks::CanAllBeEditedByUser(model_->client(), params.nodes))
      operation_ |= ui::DragDropTypes::DRAG_MOVE;
  }

  void Start(const BookmarkNode* drag_node) {
    drag_node_id_ = drag_node->id();

    gfx::ImageSkia icon;
    if (drag_node->is_url()) {
      const gfx::Image& image = model_->GetFavicon(drag_node);
      // If favicon is not loaded, the above call will initiate loading, and
      // drag will proceed in BookmarkNodeFaviconChanged(). In rare cases,
      // BookmarkNodeFaviconChanged() will never be called (e.g unfortunate
      // bookmark deletion timing) and we intentionally leak at most one request
      // in these cases which will clean up next drag.
      if (!drag_node->is_favicon_loaded())
        return;

      icon = image.AsImageSkia();
    } else {
      icon = GetBookmarkFolderIcon(
          ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
              ui::NativeTheme::kColorId_LabelEnabledColor));
    }

    OnBookmarkIconLoaded(drag_node, icon);
  }

  void OnBookmarkIconLoaded(const BookmarkNode* drag_node,
                            const gfx::ImageSkia& icon) {
    gfx::ImageSkia drag_image(
        std::make_unique<BookmarkDragImageSource>(
            drag_node->GetTitle(),
            icon.isNull()
                ? *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                      IDR_DEFAULT_FAVICON)
                : icon,
            count_),
        BookmarkDragImageSource::kBookmarkDragImageSize);

    drag_data_->provider().SetDragImage(
        drag_image, gfx::Vector2d(BookmarkDragImageSource::kDragImageOffsetX,
                                  BookmarkDragImageSource::kDragImageOffsetY));

    std::move(do_drag_callback_)
        .Run(std::move(drag_data_), native_view_, source_, start_point_,
             operation_);

    delete this;
  }

  base::WeakPtr<BookmarkDragHelper> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // bookmarks::BaseBookmarkModelObserver overrides:
  void BookmarkModelChanged() override {}

  void BookmarkModelBeingDeleted(BookmarkModel* model) override { delete this; }

  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override {
    if (node->id() != drag_node_id_)
      return;

    const gfx::Image& image = model_->GetFavicon(node);
    DCHECK(node->is_favicon_loaded());

    OnBookmarkIconLoaded(node, image.AsImageSkia());
  }

  BookmarkModel* model_;

  int64_t drag_node_id_ = -1;
  int count_;
  gfx::NativeView native_view_;
  ui::DragDropTypes::DragEventSource source_;
  const gfx::Point start_point_;
  int operation_;

  DoBookmarkDragCallback do_drag_callback_;

  std::unique_ptr<ui::OSExchangeData> drag_data_;

  ScopedObserver<bookmarks::BookmarkModel, bookmarks::BookmarkModelObserver>
      observer_;

  base::WeakPtrFactory<BookmarkDragHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkDragHelper);
};

void DoDragImpl(std::unique_ptr<ui::OSExchangeData> drag_data,
                gfx::NativeView native_view,
                ui::DragDropTypes::DragEventSource source,
                gfx::Point point,
                int operation) {
  // Allow nested run loop so we get DnD events as we drag this around.
  base::MessageLoopCurrent::ScopedNestableTaskAllower nestable_task_allower;

  views::Widget* widget = views::Widget::GetWidgetForNativeView(native_view);
  if (widget) {
    widget->RunShellDrag(nullptr, std::move(drag_data), gfx::Point(), operation,
                         source);
  } else {
    views::RunShellDrag(native_view, std::move(drag_data), point, operation,
                        source);
  }
}

void DragBookmarksImpl(Profile* profile,
                       const BookmarkDragParams& params,
                       DoBookmarkDragCallback do_drag_callback) {
  DCHECK(!params.nodes.empty());
  static base::NoDestructor<base::WeakPtr<BookmarkDragHelper>> g_drag_helper;
  if (*g_drag_helper)
    delete g_drag_helper->get();

  DCHECK(!*g_drag_helper);

  // Cleaned up in
  // BookmarkDragHelper::BookmarkIconLoaded()/BookmarkModelBeingDeleted(), or
  // above when a new drag is initiated before the favicon loads.
  *g_drag_helper =
      BookmarkDragHelper::Create(profile, params, std::move(do_drag_callback));
}

}  // namespace

void DragBookmarks(Profile* profile, const BookmarkDragParams& params) {
  DragBookmarksImpl(profile, params, base::BindOnce(&DoDragImpl));
}

void DragBookmarksForTest(Profile* profile,
                          const BookmarkDragParams& params,
                          DoBookmarkDragCallback do_drag_callback) {
  DragBookmarksImpl(profile, params, std::move(do_drag_callback));
}

}  // namespace chrome
