// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/drag_and_drop_test_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/android/jar_jni/DragEvent_jni.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "net/base/mime_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"

namespace drag_and_drop_test_utils {

namespace {

// Walks up the ViewAndroid hierarchy starting at `view` to find the
// `ui::EventForwarder` attached to the root ContentView, which routes drag
// events back down into `target_contents`'s `WebContentsViewAndroid`.
ui::EventForwarder* FindEventForwarder(ui::ViewAndroid* view) {
  for (ui::ViewAndroid* curr = view; curr; curr = curr->parent()) {
    if (ui::EventForwarder* forwarder = curr->event_forwarder()) {
      return forwarder;
    }
  }
  return nullptr;
}

}  // namespace

struct DragAndDropSimulator::PlatformState {
  PlatformState(content::WebContents* drag_contents,
                content::WebContents* drop_contents)
      : drag_contents_(drag_contents), drop_contents_(drop_contents) {}

  bool SendDragEvent(content::WebContents* target_contents,
                     int action,
                     const gfx::Point& location) {
    if (!target_contents) {
      return false;
    }
    ui::ViewAndroid* view = target_contents->GetContentNativeView();
    if (!view) {
      return false;
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    float dip_scale = view->GetDipScale();
    float x_px = location.x() * dip_scale;
    float y_px = location.y() * dip_scale;
    gfx::PointF screen_px = view->GetLocationOnScreen(x_px, y_px);

    base::android::ScopedJavaLocalRef<jobjectArray> j_mime_types;
    base::android::ScopedJavaLocalRef<jstring> j_text;
    base::android::ScopedJavaLocalRef<jstring> j_html;
    base::android::ScopedJavaLocalRef<jstring> j_url;
    base::android::ScopedJavaLocalRef<jobjectArray> j_filenames;
    base::android::ScopedJavaLocalRef<jstring> j_custom_data;
    base::android::ScopedJavaLocalRef<jstring> j_effect_allowed;
    bool has_files = false;

    if (active_drop_data_) {
      const bool is_drop = (action == DragEventJni::ACTION_DROP);
      std::vector<std::u16string> mime_types;
      if (active_drop_data_->text) {
        mime_types.push_back(ui::kMimeTypePlainText16);
        if (is_drop) {
          j_text = base::android::ConvertUTF16ToJavaString(
              env, *active_drop_data_->text);
        }
      }
      if (!active_drop_data_->url_infos.empty()) {
        mime_types.push_back(ui::kMimeTypeMozillaUrl16);
        if (is_drop) {
          j_url = base::android::ConvertUTF8ToJavaString(
              env, active_drop_data_->url_infos.front().url.spec());
        }
      }
      if (!active_drop_data_->filenames.empty()) {
        has_files = true;
        base::ScopedAllowBlockingForTesting allow_blocking;
        std::vector<std::vector<std::string>> file_matrix;
        for (const auto& file_info : active_drop_data_->filenames) {
          std::string mime_type = ui::kMimeTypeOctetStream;
          net::GetMimeTypeFromFile(file_info.path, &mime_type);
          mime_types.push_back(base::UTF8ToUTF16(mime_type));
          if (is_drop) {
            file_matrix.push_back(
                {file_info.path.value(), file_info.display_name.value()});
          }
        }
        if (is_drop) {
          j_filenames =
              base::android::ToJavaArrayOfStringArray(env, file_matrix);
        }
      }
      j_mime_types = base::android::ToJavaArrayOfStrings(env, mime_types);
    }

    ui::EventForwarder* forwarder = FindEventForwarder(view);
    if (!forwarder) {
      // `GetEventForwarder()` lazily initializes `view->event_forwarder_` and
      // its Java `EventForwarder` peer if none exists yet on the ViewAndroid
      // hierarchy.
      view->GetEventForwarder();
      forwarder = view->event_forwarder();
    }
    if (!forwarder) {
      return false;
    }

    forwarder->OnDragEvent(env, action, x_px, y_px, screen_px.x(),
                           screen_px.y(), j_mime_types, has_files, j_filenames,
                           j_text, j_html, j_url, j_custom_data,
                           j_effect_allowed);
    return true;
  }

  raw_ptr<content::WebContents> drag_contents_ = nullptr;
  raw_ptr<content::WebContents> drop_contents_ = nullptr;
  std::unique_ptr<content::DropData> active_drop_data_;
};

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* web_contents)
    : DragAndDropSimulator(web_contents, web_contents) {}

DragAndDropSimulator::DragAndDropSimulator(content::WebContents* drag_contents,
                                           content::WebContents* drop_contents)
    : state_(std::make_unique<PlatformState>(drag_contents, drop_contents)) {}

DragAndDropSimulator::~DragAndDropSimulator() = default;

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const std::string& text) {
  if (state_->active_drop_data_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  state_->active_drop_data_->text = base::UTF8ToUTF16(text);
  return state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_ENTERED, location) &&
         state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_LOCATION, location);
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const GURL& url) {
  if (state_->active_drop_data_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  state_->active_drop_data_->url_infos.emplace_back(
      url, base::UTF8ToUTF16(url.spec()));
  state_->active_drop_data_->text = base::UTF8ToUTF16(url.spec());
  return state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_ENTERED, location) &&
         state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_LOCATION, location);
}

bool DragAndDropSimulator::SimulateDragEnter(const gfx::Point& location,
                                             const base::FilePath& file) {
  if (state_->active_drop_data_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  state_->active_drop_data_->filenames.emplace_back(file, file.BaseName());
  return state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_ENTERED, location) &&
         state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_LOCATION, location);
}

bool DragAndDropSimulator::SimulateDragEnter(
    const gfx::Point& location,
    const std::vector<ui::FileInfo>& file_infos) {
  if (state_->active_drop_data_) {
    ADD_FAILURE() << "Cannot start a new drag when old one hasn't ended yet.";
    return false;
  }
  state_->active_drop_data_ = std::make_unique<content::DropData>();
  for (const auto& file_info : file_infos) {
    state_->active_drop_data_->filenames.emplace_back(file_info.path,
                                                      file_info.display_name);
  }
  return state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_ENTERED, location) &&
         state_->SendDragEvent(state_->drag_contents_,
                               DragEventJni::ACTION_DRAG_LOCATION, location);
}

bool DragAndDropSimulator::SimulateDrop(const gfx::Point& location) {
  if (!state_->active_drop_data_) {
    ADD_FAILURE() << "Cannot drop a drag that hasn't started yet.";
    return false;
  }

  // TODO(crbug.com/508693696): Android does not support web-to-Glic drags
  // across separate WebContents yet. If `drag_contents_` and `drop_contents_`
  // differ in future cross-WebContents tests, ensure `drop_contents_` receives
  // `ACTION_DRAG_ENTERED` to initialize its drag metadata before updating
  // location and dropping.
  if (state_->drag_contents_ != state_->drop_contents_) {
    if (!state_->SendDragEvent(state_->drop_contents_,
                               DragEventJni::ACTION_DRAG_ENTERED, location)) {
      state_->active_drop_data_.reset();
      return false;
    }
  }

  // Refresh drag location at the target coordinates immediately prior to
  // dropping.
  if (!state_->SendDragEvent(state_->drop_contents_,
                             DragEventJni::ACTION_DRAG_LOCATION, location)) {
    state_->active_drop_data_.reset();
    return false;
  }

  bool success =
      state_->SendDragEvent(state_->drop_contents_, DragEventJni::ACTION_DROP,
                            location) &&
      state_->SendDragEvent(state_->drop_contents_,
                            DragEventJni::ACTION_DRAG_ENDED, location);
  state_->active_drop_data_.reset();
  return success;
}

}  // namespace drag_and_drop_test_utils
