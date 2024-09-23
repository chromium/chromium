// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_THUMBNAIL_LOADER_THUMBNAIL_LOADER_H_
#define CHROME_BROWSER_UI_ASH_THUMBNAIL_LOADER_THUMBNAIL_LOADER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "ui/gfx/geometry/size.h"

class Profile;
class SkBitmap;

namespace base {
class FilePath;
}

namespace ash {

// Loader for file-backed thumbnails. It opens a native connection to the image
// loader extension (also used to generate thumbnails for the file manager), and
// sends it an image request for a file path. It decodes data returned by the
// extension into a bitmap.
class ThumbnailLoader {
 public:
  explicit ThumbnailLoader(Profile* profile);
  ThumbnailLoader(const ThumbnailLoader&) = delete;
  ThumbnailLoader& operator=(const ThumbnailLoader&) = delete;
  virtual ~ThumbnailLoader();

  // Thumbnail request data that will be forwarded to the image loader.
  struct ThumbnailRequest {
    ThumbnailRequest(const base::FilePath& file_path, const gfx::Size& size);
    ~ThumbnailRequest();

    // The absolute file path.
    const base::FilePath file_path;

    // The desired bitmap size.
    const gfx::Size size;
  };

  // Returns a weak pointer to this instance.
  base::WeakPtr<ThumbnailLoader> GetWeakPtr();

  using ImageCallback =
      base::OnceCallback<void(const SkBitmap* bitmap, base::File::Error error)>;
  // Starts a request for a thumbnail. `callback` called with the generated
  // bitmap. On error, the bitmap will be null.
  virtual void Load(const ThumbnailRequest& request, ImageCallback callback);

 private:
  class ThumbnailDecoder;

  // Starts thumbnail request after the file metadata has been retrieved. The
  // metadata is used to verify that the path exists, points to a file, and to
  // get the file's last modification time.
  void LoadForFileWithMetadata(const ThumbnailRequest& request,
                               ImageCallback callback,
                               base::File::Error result,
                               const base::File::Info& file_info);

  // Callback to the image loader request.
  // `request_id` identifies the thumbnail request.
  // `requested_size` - size of the thumbnail that was requested.
  // `data` - image data returned by the image loader. Expected to be in a data
  // URL form. It will attempt to decode the received data.
  void OnThumbnailLoaded(const base::UnguessableToken& request_id,
                         const gfx::Size& requested_size,
                         const std::string& data);

  // Finalizes the thumbnail request identified by `request_id`. It invokes the
  // request callback with `bitmap`. If `bitmap` size is larger than the
  // originally `requested_size`, the bitmap will be cropped.
  void RespondToRequest(const base::UnguessableToken& request_id,
                        const gfx::Size& requested_size,
                        const SkBitmap* bitmap,
                        base::File::Error error);

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  // Maps pending thumbnail requests to their registered callbacks.
  std::map<base::UnguessableToken, ImageCallback> requests_;

  // Maps pending thumbnail requests to image decoders used to transform data
  // received from the image loder into a bitmap.
  std::map<base::UnguessableToken, std::unique_ptr<ThumbnailDecoder>>
      thumbnail_decoders_;

  base::WeakPtrFactory<ThumbnailLoader> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_THUMBNAIL_LOADER_THUMBNAIL_LOADER_H_
