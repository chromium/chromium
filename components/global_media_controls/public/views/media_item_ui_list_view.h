// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_LIST_VIEW_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_LIST_VIEW_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"

namespace global_media_controls {

class MediaItemUIView;

// MediaItemUIListView is a container that holds a list of MediaItemUIViews and
// handles adding/removing separators and creating a scrollable view.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIListView
    : public views::ScrollView {
 public:
  METADATA_HEADER(MediaItemUIListView);
  struct SeparatorStyle {
    SeparatorStyle(SkColor separator_color, int separator_thickness);

    const SkColor separator_color;
    const int separator_thickness;
  };

  explicit MediaItemUIListView(
      const absl::optional<SeparatorStyle>& separator_style,
      bool should_clip_height);
  MediaItemUIListView();
  MediaItemUIListView(const MediaItemUIListView&) = delete;
  MediaItemUIListView& operator=(const MediaItemUIListView&) = delete;
  ~MediaItemUIListView() override;

  // Adds the given item into the list.
  void ShowItem(const std::string& id, std::unique_ptr<MediaItemUIView> item);

  // Removes the given item from the list.
  void HideItem(const std::string& id);

  bool empty() { return items_.empty(); }

  base::WeakPtr<MediaItemUIListView> GetWeakPtr();

  const std::map<const std::string, MediaItemUIView*>& items_for_testing()
      const {
    return items_;
  }

 private:
  std::map<const std::string, MediaItemUIView*> items_;

  absl::optional<SeparatorStyle> separator_style_;

  base::WeakPtrFactory<MediaItemUIListView> weak_factory_{this};
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_LIST_VIEW_H_
