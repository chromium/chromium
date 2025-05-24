// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_FACTORY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"

class Profile;

namespace web_package {
class SignedWebBundleId;
class SignedWebBundleIntegrityBlock;
}  // namespace web_package

namespace web_app {

// Factory for creating instances of `IsolatedWebAppResponseReader` that are
// ready to read responses from the bundle. Instances returned by this class are
// guaranteed to have previously read a valid integrity block and metadata, as
// well as to have verified that the signatures are valid (unless
// `skip_signature_verification` is set).
class IsolatedWebAppResponseReaderFactory {
 public:
  explicit IsolatedWebAppResponseReaderFactory(Profile& profile);
  virtual ~IsolatedWebAppResponseReaderFactory();

  IsolatedWebAppResponseReaderFactory(
      const IsolatedWebAppResponseReaderFactory&) = delete;
  IsolatedWebAppResponseReaderFactory& operator=(
      const IsolatedWebAppResponseReaderFactory&) = delete;

  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                     UnusableSwbnFileError>)>;

  enum class Flag {
    kMinValue,
    kDevModeBundle = kMinValue,
    kSkipSignatureVerification,
    kMaxValue = kSkipSignatureVerification
  };
  using Flags = base::EnumSet<Flag, Flag::kMinValue, Flag::kMaxValue>;

  virtual void CreateResponseReader(
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      Flags flags,
      Callback callback);

  static std::string ErrorToString(const UnusableSwbnFileError& error);

 private:
  void CreateResponseReaderImpl(
      const base::FilePath& web_bundle_path,
      const web_package::SignedWebBundleId& web_bundle_id,
      Flags flags,
      Callback callback);

  void OnReaderCreated(const base::FilePath& web_bundle_path,
                       const web_package::SignedWebBundleId& web_bundle_id,
                       Flags flags,
                       Callback callback,
                       base::expected<std::unique_ptr<SignedWebBundleReader>,
                                      UnusableSwbnFileError> status);

  const raw_ref<Profile> profile_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IsolatedWebAppResponseReaderFactory> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_RESPONSE_READER_FACTORY_H_
