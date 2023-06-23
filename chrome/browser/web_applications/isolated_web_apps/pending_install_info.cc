// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {
namespace {

class PendingInstallInfoHolder
    : public content::WebContentsUserData<PendingInstallInfoHolder> {
 public:
  explicit PendingInstallInfoHolder(
      content::WebContents* web_contents,
      std::unique_ptr<IsolatedWebAppPendingInstallInfo> pending_install_info)
      : content::WebContentsUserData<PendingInstallInfoHolder>(*web_contents),
        pending_install_info_(std::move(pending_install_info)) {}

  IsolatedWebAppPendingInstallInfo& pending_install_info() const {
    return *pending_install_info_;
  }

 private:
  std::unique_ptr<IsolatedWebAppPendingInstallInfo> pending_install_info_;

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PendingInstallInfoHolder);

}  // namespace

/*static*/ IsolatedWebAppPendingInstallInfo&
IsolatedWebAppPendingInstallInfo::FromWebContents(
    content::WebContents& web_contents) {
  auto* holder = PendingInstallInfoHolder::FromWebContents(&web_contents);

  if (holder == nullptr) {
    PendingInstallInfoHolder::CreateForWebContents(
        &web_contents,
        base::WrapUnique(new IsolatedWebAppPendingInstallInfo{}));
    holder = PendingInstallInfoHolder::FromWebContents(&web_contents);
  }
  DCHECK(holder != nullptr);
  return holder->pending_install_info();
}

IsolatedWebAppPendingInstallInfo::IsolatedWebAppPendingInstallInfo() = default;
IsolatedWebAppPendingInstallInfo::~IsolatedWebAppPendingInstallInfo() = default;

void IsolatedWebAppPendingInstallInfo::set_isolated_web_app_location(
    const IsolatedWebAppLocation& location) {
  location_ = location;
}

const absl::optional<IsolatedWebAppLocation>&
IsolatedWebAppPendingInstallInfo::location() const {
  return location_;
}

void IsolatedWebAppPendingInstallInfo::ResetIsolatedWebAppLocation() {
  location_ = absl::nullopt;
}

}  // namespace web_app
