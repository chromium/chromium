// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ICON_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ICON_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace apps {
class ArcIconOnceLoader;
}

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

// A class that provides an ImageSkia for UI code to use. It handles ARC app
// icon resource loading, screen scale factor change etc. UI code that uses
// ARC app icon should host this class.
//
// Icon images are sometimes subject to post-processing effects, such as
// desaturating (graying out) disabled apps. Applying those effects are the
// responsibility of code that uses this ArcAppIcon class, not the
// responsibility of ArcAppIcon itself.
class ArcAppIcon {
 public:
  class Observer {
   public:
    // Invoked when a new image rep for an additional scale factor
    // is loaded and added to |image|.
    virtual void OnIconUpdated(ArcAppIcon* icon) = 0;

   protected:
    virtual ~Observer() {}
  };

  ArcAppIcon(content::BrowserContext* context,
             const std::string& app_id,
             int resource_size_in_dip,
             Observer* observer,
             bool serve_compressed_icons = false);
  ~ArcAppIcon();

  // Starts loading the icon at every supported scale factor. The |observer_|
  // will be notified as progress is made. "Supported" is in the same sense as
  // ui::GetSupportedScaleFactors().
  void LoadSupportedScaleFactors();

  // Whether every supported scale factor was successfully loaded. "Supported"
  // is in the same sense as ui::GetSupportedScaleFactors().
  bool EverySupportedScaleFactorIsLoaded() const;

  const std::string& app_id() const { return app_id_; }
  // Valid if the |serve_compressed_icons_| is false.
  const gfx::ImageSkia& image_skia() const {
    DCHECK(!serve_compressed_icons_);
    return image_skia_;
  }
  // Valid if the |serve_compressed_icons_| is true.
  const std::map<ui::ScaleFactor, std::string>& compressed_images() const {
    DCHECK(serve_compressed_icons_);
    return compressed_images_;
  }

  // Disables async safe decoding requests when unit tests are executed. This is
  // done to avoid two problems. Problems come because icons are decoded at a
  // separate process created by ImageDecoder. ImageDecoder has 5 seconds delay
  // to stop since the last request (see its kBatchModeTimeoutSeconds for more
  // details). This is unacceptably long for unit tests because the test
  // framework waits until external process is finished. Another problem happens
  // when we issue a decoding request, but the process has not started its
  // processing yet by the time when a test exits. This might cause situation
  // when g_one_utility_thread_lock from in_process_utility_thread.cc gets
  // released in an acquired state which is crash condition in debug builds.
  static void DisableSafeDecodingForTesting();
  static bool IsSafeDecodingDisabledForTesting();

 private:
  friend class ArcAppIconLoader;
  friend class apps::ArcIconOnceLoader;

  class Source;
  class DecodeRequest;
  struct ReadResult;

  // Icon loading is performed in several steps. It is initiated by
  // LoadImageForScaleFactor request that specifies a required scale factor.
  // ArcAppListPrefs is used to resolve a path to resource. Content of file is
  // asynchronously read in context of browser file thread. On successful read,
  // an icon data is decoded to an image in the special utility process.
  // DecodeRequest is used to interact with the utility process, and each
  // active request is stored at |decode_requests_| vector. When decoding is
  // complete, results are returned in context of UI thread, and corresponding
  // request is removed from |decode_requests_|. In case of some requests are
  // not completed by the time of deleting this icon, they are automatically
  // canceled.
  // In case of the icon file is not available this requests ArcAppListPrefs to
  // install required resource from ARC side. ArcAppListPrefs notifies UI items
  // that new icon is available and corresponding item should invoke
  // LoadImageForScaleFactor again.
  void LoadForScaleFactor(ui::ScaleFactor scale_factor);

  void MaybeRequestIcon(ui::ScaleFactor scale_factor);
  static std::unique_ptr<ArcAppIcon::ReadResult> ReadOnFileThread(
      ui::ScaleFactor scale_factor,
      const base::FilePath& path,
      const base::FilePath& default_app_path);
  void OnIconRead(std::unique_ptr<ArcAppIcon::ReadResult> read_result);
  void UpdateUncompressed(ui::ScaleFactor scale_factor, const SkBitmap& bitmap);
  void UpdateCompressed(ui::ScaleFactor scale_factor, std::string data);
  void DiscardDecodeRequest(DecodeRequest* request);

  content::BrowserContext* const context_;
  const std::string app_id_;
  // Contains app id that is actually used to read an icon resource to support
  // shelf group mapping to shortcut.
  const std::string mapped_app_id_;
  const int resource_size_in_dip_;
  Observer* const observer_;
  const bool serve_compressed_icons_;
  // Used to separate first 5 loaded app icons and other app icons.
  // Only one form of app icons will be loaded, compressed or uncompressed, so
  // only one counter is needed.
  int icon_loaded_count_ = 0;

  gfx::ImageSkia image_skia_;
  std::map<ui::ScaleFactor, std::string> compressed_images_;
  std::map<ui::ScaleFactor, base::Time> incomplete_scale_factors_;

  // Contains pending image decode requests.
  std::vector<std::unique_ptr<DecodeRequest>> decode_requests_;

  base::WeakPtrFactory<ArcAppIcon> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAppIcon);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ICON_H_
