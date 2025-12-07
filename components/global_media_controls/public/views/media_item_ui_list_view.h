// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_LIST_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_LIST_VIEW_H_

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"

namespace global_media_controls {

class MediaItemUIUpdatedView;
class MediaItemUIView;

// MediaItemUIListView is a scrollable view container that holds a list of
// MediaItemUIViews or MediaItemUIUpdatedViews, and creates item separators if
// needed.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIListView
    : public views::ScrollView {
  METADATA_HEADER(MediaItemUIListView, views::ScrollView)

 public:
  struct SeparatorStyle {
    SeparatorStyle(SkColor separator_color, int separator_thickness);

    const SkColor separator_color;
    const int separator_thickness;
  };

  explicit MediaItemUIListView(
      const std::optional<SeparatorStyle>& separator_style,
      bool should_clip_height);
  MediaItemUIListView();
  MediaItemUIListView(const MediaItemUIListView&) = delete;
  MediaItemUIListView& operator=(const MediaItemUIListView&) = delete;
  ~MediaItemUIListView() override;

  // Adds the given MediaItemUIView into the list.
  void ShowItem(const std::string& id, std::unique_ptr<MediaItemUIView> item);

  // Removes the given MediaItemUIView from the list.
  void HideItem(const std::string& id);

  // Gets the given MediaItemUIView from the list.
  MediaItemUIView* GetItem(const std::string& id);

  // Adds the given MediaItemUIUpdatedView into the list.
  void ShowUpdatedItem(const std::string& id,
                       std::unique_ptr<MediaItemUIUpdatedView> item);

  // Removes the given MediaItemUIUpdatedView from the list.
  void HideUpdatedItem(const std::string& id);

  // Gets the given MediaItemUIUpdatedView from the list.
  MediaItemUIUpdatedView* GetUpdatedItem(const std::string& id);

  bool empty() { return items_.empty() && updated_items_.empty(); }

  base::WeakPtr<MediaItemUIListView> GetWeakPtr();

  const std::map<const std::string, raw_ptr<MediaItemUIView, CtnExperimental>>&
  items_for_testing() const {
    return items_;
  }

  const std::map<const std::string,
                 raw_ptr<MediaItemUIUpdatedView, CtnExperimental>>&
  updated_items_for_testing() const {
    return updated_items_;
  }

 private:
  // If media::kGlobalMediaControlsUpdatedUI on non-CrOS is enabled,
  // `updated_items_` is used, otherwise `items_` is used. `items_` is always
  // used for CrOS.
  // TODO(b/329160058): Use better naming.
  std::map<const std::string, raw_ptr<MediaItemUIView, CtnExperimental>> items_;
  std::map<const std::string, raw_ptr<MediaItemUIUpdatedView, CtnExperimental>>
      updated_items_;

  std::optional<SeparatorStyle> separator_style_;

  base::WeakPtrFactory<MediaItemUIListView> weak_factory_{this};
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_LIST_VIEW_H_
