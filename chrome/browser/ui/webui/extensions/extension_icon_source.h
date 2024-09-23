// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ICON_SOURCE_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"

class ExtensionIconSet;
class Profile;
class SkBitmap;

namespace extensions {
class Extension;

// ExtensionIconSource serves extension icons through network level chrome:
// requests. Icons can be retrieved for any installed extension or app.
//
// The format for requesting an icon is as follows:
//   chrome://extension-icon/<extension_id>/<icon_size>/<match_type>?[options]
//
//   Parameters (<> required, [] optional):
//    <extension_id>  = the id of the extension
//    <icon_size>     = the size of the icon, as the integer value of the
//                      corresponding Extension:Icons enum.
//    <match_type>    = the fallback matching policy, as the integer value of
//                      the corresponding ExtensionIconSet::Match enum.
//    [options]       = Optional transformations to apply. Supported options:
//                        grayscale=true to desaturate the image.
//
// Examples:
//   chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/32/1?grayscale=true
//     (ICON_SMALL, kBigger, grayscale)
//   chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/128/0
//     (ICON_LARGE, kExactly)
//
// We attempt to load icons from the following sources in order:
//  1) The icons as listed in the extension / app manifests.
//  2) If a 16px icon was requested, the favicon for extension's launch URL.
//  3) The default extension / application icon if there are still no matches.
//
class ExtensionIconSource : public content::URLDataSource {
 public:
  explicit ExtensionIconSource(Profile* profile);

  ExtensionIconSource(const ExtensionIconSource&) = delete;
  ExtensionIconSource& operator=(const ExtensionIconSource&) = delete;

  ~ExtensionIconSource() override;

  // Gets the URL of the |extension| icon in the given |icon_size|, falling back
  // based on the |match| type. If |grayscale|, the URL will be for the
  // desaturated version of the icon.
  static GURL GetIconURL(const Extension* extension,
                         int icon_size,
                         ExtensionIconSet::Match match,
                         bool grayscale);
  static GURL GetIconURL(const std::string& extension_id,
                         int icon_size,
                         ExtensionIconSet::Match match,
                         bool grayscale);

  // A public utility function for accessing the bitmap of the image specified
  // by |resource_id|.
  static SkBitmap* LoadImageByResourceId(int resource_id);

  // content::URLDataSource implementation.
  std::string GetSource() override;
  std::string GetMimeType(const GURL&) override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  bool AllowCaching() override;

 private:
  // Encapsulates the request parameters for |request_id|.
  struct ExtensionIconRequest;

  // Returns the bitmap for the default app image.
  const SkBitmap* GetDefaultAppImage();

  // Returns the bitmap for the default extension.
  const SkBitmap* GetDefaultExtensionImage();

  // Performs any remaining transformations (like desaturating the |image|),
  // then returns the |image| to the client and clears up any temporary data
  // associated with the |request_id|.
  void FinalizeImage(const SkBitmap* image, int request_id);

  // Loads the default image for |request_id| and returns to the client.
  void LoadDefaultImage(int request_id);

  // Loads the extension's |icon| for the given |request_id| and returns the
  // image to the client.
  void LoadExtensionImage(const ExtensionResource& icon,
                          int request_id);

  // Loads the favicon image for the app associated with the |request_id|. If
  // the image does not exist, we fall back to the default image.
  void LoadFaviconImage(int request_id);

  // FaviconService callback
  void OnFaviconDataAvailable(
      int request_id,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // ImageLoader callback
  void OnImageLoaded(int request_id, const gfx::Image& image);

  // Called when the extension doesn't have an icon. We fall back to multiple
  // sources, using the following order:
  //  1) The icons as listed in the extension / app manifests.
  //  2) If a 16px icon and the extension has a launch URL, see if Chrome
  //     has a corresponding favicon.
  //  3) If still no matches, load the default extension / application icon.
  void LoadIconFailed(int request_id);

  // Parses and saves an ExtensionIconRequest for the URL |path| for the
  // specified |request_id|. Takes the |callback| if it returns true.
  bool ParseData(const std::string& path,
                 int request_id,
                 content::URLDataSource::GotDataCallback* callback);

  // Stores the parameters associated with the |request_id|, making them
  // as an ExtensionIconRequest via GetData.
  void SetData(int request_id,
               content::URLDataSource::GotDataCallback callback,
               const Extension* extension,
               bool grayscale,
               int size,
               ExtensionIconSet::Match match);

  // Returns the ExtensionIconRequest for the given |request_id|.
  ExtensionIconRequest* GetData(int request_id);

  // Removes temporary data associated with |request_id|.
  void ClearData(int request_id);

  raw_ptr<Profile, FlakyDanglingUntriaged> profile_;

  // Maps tracker ids to request ids.
  std::map<int, int> tracker_map_;

  // Maps request_ids to ExtensionIconRequests.
  std::map<int, std::unique_ptr<ExtensionIconRequest>> request_map_;

  std::unique_ptr<SkBitmap> default_app_data_;

  std::unique_ptr<SkBitmap> default_extension_data_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<ExtensionIconSource> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ICON_SOURCE_H_
