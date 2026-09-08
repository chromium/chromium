// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_USER_IMAGE_USER_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_USER_IMAGE_USER_IMAGE_SOURCE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/url_data_source.h"

class AccountId;
class PrefService;

namespace base {
class RefCountedMemory;
}

namespace ash {

// UserImageSource is the data source that serves user images for users that
// have it.
class UserImageSource : public content::URLDataSource {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit UserImageSource(PrefService* local_state);

  UserImageSource(const UserImageSource&) = delete;
  UserImageSource& operator=(const UserImageSource&) = delete;

  ~UserImageSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;

  // Returns PNG encoded image for user with specified |account_id|. If there's
  // no user with such an id, returns the first default image. Always returns
  // the 100%-scale asset.
  static scoped_refptr<base::RefCountedMemory> GetUserImage(
      const AccountId& account_id);

 private:
  const raw_ref<PrefService> local_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_USER_IMAGE_USER_IMAGE_SOURCE_H_
