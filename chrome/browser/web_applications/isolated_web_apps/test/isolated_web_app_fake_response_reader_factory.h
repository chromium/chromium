// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_FAKE_RESPONSE_READER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_FAKE_RESPONSE_READER_FACTORY_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

class Profile;

namespace web_app {

class MockIsolatedWebAppResponseReader : public IsolatedWebAppResponseReader {
 public:
  ~MockIsolatedWebAppResponseReader() override = default;
  web_package::SignedWebBundleIntegrityBlock GetIntegrityBlock() override;
  void ReadResponse(const network::ResourceRequest& resource_request,
                    ReadResponseCallback callback) override;
  void Close(base::OnceClosure callback) override;
};

class FakeResponseReaderFactory : public IsolatedWebAppResponseReaderFactory {
 public:
  explicit FakeResponseReaderFactory(
      Profile& profile,
      base::expected<void, UnusableSwbnFileError> bundle_status);
  ~FakeResponseReaderFactory() override;

  void CreateResponseReader(const base::FilePath& web_bundle_path,
                            const web_package::SignedWebBundleId& web_bundle_id,
                            IsolatedWebAppResponseReaderFactory::Flags flags,
                            Callback callback) override;

 private:
  base::expected<void, UnusableSwbnFileError> bundle_status_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_FAKE_RESPONSE_READER_FACTORY_H_
