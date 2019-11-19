// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_ICON_H_
#define CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_ICON_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class FilePath;
}

namespace crostini {
class CrostiniRegistryService;
}

class Profile;

// A class that provides an ImageSkia for UI code to use. It handles Crostini
// app icon resource loading, screen scale factor change etc. UI code that uses
// Crostini app icon should host this class.
class CrostiniAppIcon {
 public:
  class Observer {
   public:
    // Invoked when a new image rep for an additional scale factor
    // is loaded and added to |image|.
    virtual void OnIconUpdated(CrostiniAppIcon* icon) = 0;

   protected:
    virtual ~Observer() {}
  };

  CrostiniAppIcon(Profile* profile,
                  const std::string& app_id,
                  int resource_size_in_dip,
                  Observer* observer);
  ~CrostiniAppIcon();

  const std::string& app_id() const { return app_id_; }
  const gfx::ImageSkia& image_skia() const { return image_skia_; }

  // Icon loading is performed in several steps. It is initiated by
  // LoadImageForScaleFactor request that specifies a required scale factor.
  // CrostiniRegistryService is used to resolve a path to resource. Content of
  // file is asynchronously read in context of browser file thread. On
  // successful read, an icon data is decoded to an image in the special utility
  // process. DecodeRequest is used to interact with the utility process, and
  // each active request is stored at |decode_requests_| vector. When decoding
  // is complete, results are returned in context of UI thread, and
  // corresponding request is removed from |decode_requests_|. In case of some
  // requests are not completed by the time of deleting this icon, they are
  // automatically canceled. In case of the icon file is not available this
  // requests CrostiniRegistryService to install required resource from Crostini
  // side. CrostiniRegistryService notifies UI items that new icon is available
  // and corresponding item should invoke LoadImageForScaleFactor again.
  void LoadForScaleFactor(ui::ScaleFactor scale_factor);

 private:
  class Source;
  class DecodeRequest;
  struct ReadResult;

  // Makes a request back to the registry service to request an icon if one
  // isn't already in progress.
  void MaybeRequestIcon(ui::ScaleFactor scale_factor);
  // Reads the icon data in and returns the result.
  static std::unique_ptr<CrostiniAppIcon::ReadResult> ReadOnFileThread(
      ui::ScaleFactor scale_factor,
      const base::FilePath& path);
  // Callback handler for when we have read an icon file from storage.
  void OnIconRead(std::unique_ptr<CrostiniAppIcon::ReadResult> read_result);
  // Called when we have updated an icon and we should notify our observer
  // about the change.
  void Update(ui::ScaleFactor scale_factor, const SkBitmap& bitmap);
  // Removed the corresponding |request| from our list.
  void DiscardDecodeRequest(DecodeRequest* request);

  base::WeakPtr<crostini::CrostiniRegistryService> registry_service_;
  const std::string app_id_;
  const int resource_size_in_dip_;
  Observer* const observer_;

  gfx::ImageSkia image_skia_;

  // TODO(jkardatzke): Remove this for M-72, it's to cleanup from
  // crbug.com/891588
  std::set<ui::ScaleFactor> already_requested_icons_;

  // Contains pending image decode requests.
  std::vector<std::unique_ptr<DecodeRequest>> decode_requests_;

  base::WeakPtrFactory<CrostiniAppIcon> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppIcon);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_ICON_H_
