// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/fake_installable_manager.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

FakeInstallableManager::FakeInstallableManager(
    content::WebContents* web_contents)
    : InstallableManager(web_contents),
      manifest_(blink::mojom::Manifest::New()),
      web_page_metadata_(mojom::WebPageMetadata::New()) {}

FakeInstallableManager::~FakeInstallableManager() = default;

void FakeInstallableManager::GetData(const InstallableParams& params,
                                     InstallableCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeInstallableManager::RunCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeInstallableManager::RunCallback(InstallableCallback callback) {
  std::move(callback).Run(*data_);
}

// static
FakeInstallableManager* FakeInstallableManager::CreateForWebContents(
    content::WebContents* web_contents) {
  auto manager = std::make_unique<FakeInstallableManager>(web_contents);
  FakeInstallableManager* result = manager.get();
  web_contents->SetUserData(UserDataKey(), std::move(manager));
  return result;
}

// static
FakeInstallableManager*
FakeInstallableManager::CreateForWebContentsWithManifest(
    content::WebContents* web_contents,
    InstallableStatusCode installable_code,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  DCHECK(manifest);
  FakeInstallableManager* installable_manager =
      FakeInstallableManager::CreateForWebContents(web_contents);

  const bool valid_manifest = !blink::IsEmptyManifest(manifest);
  installable_manager->manifest_url_ = manifest_url;
  installable_manager->manifest_ = std::move(manifest);

  std::vector<InstallableStatusCode> errors;

  // Not used:
  const std::unique_ptr<SkBitmap> icon;

  if (installable_code != InstallableStatusCode::NO_ERROR_DETECTED) {
    errors.push_back(installable_code);
  }

  auto installable_data = std::make_unique<InstallableData>(
      std::move(errors), installable_manager->manifest_url_,
      *installable_manager->manifest_, *installable_manager->web_page_metadata_,
      GURL(), icon.get(), false, std::vector<Screenshot>(), valid_manifest);

  installable_manager->data_ = std::move(installable_data);

  return installable_manager;
}

}  // namespace webapps
