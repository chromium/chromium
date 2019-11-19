// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/launcher_search_provider/launcher_search_provider_service.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_icon_image_loader_impl.h"

using chromeos::launcher_search_provider::Service;

namespace {

const char kResultIdDelimiter = ':';

}  // namespace

namespace app_list {

LauncherSearchResult::LauncherSearchResult(
    const std::string& item_id,
    const GURL& icon_url,
    const int discrete_value_relevance,
    Profile* profile,
    const extensions::Extension* extension,
    std::unique_ptr<chromeos::launcher_search_provider::ErrorReporter>
        error_reporter)
    : item_id_(item_id),
      discrete_value_relevance_(discrete_value_relevance),
      profile_(profile),
      extension_(extension) {
  DCHECK_GE(discrete_value_relevance, 0);
  DCHECK_LE(discrete_value_relevance,
            chromeos::launcher_search_provider::kMaxSearchResultScore);

  icon_image_loader_ = base::MakeRefCounted<LauncherSearchIconImageLoaderImpl>(
      icon_url, profile, extension,
      ash::AppListConfig::instance().GetPreferredIconDimension(display_type()),
      std::move(error_reporter));
  icon_image_loader_->LoadResources();

  Initialize();
}

LauncherSearchResult::~LauncherSearchResult() {
  icon_image_loader_->RemoveObserver(this);
}

std::unique_ptr<LauncherSearchResult> LauncherSearchResult::Duplicate() const {
  LauncherSearchResult* duplicated_result =
      new LauncherSearchResult(item_id_, discrete_value_relevance_, profile_,
                               extension_, icon_image_loader_);
  duplicated_result->set_model_updater(model_updater());
  duplicated_result->SetMetadata(CloneMetadata());
  return base::WrapUnique(duplicated_result);
}

void LauncherSearchResult::Open(int event_flags) {
  Service* service = Service::Get(profile_);
  service->OnOpenResult(extension_->id(), item_id_);
}

ash::SearchResultType LauncherSearchResult::GetSearchResultType() const {
  return ash::LAUNCHER_SEARCH_PROVIDER_RESULT;
}

void LauncherSearchResult::OnIconImageChanged(
    LauncherSearchIconImageLoader* image_loader) {
  DCHECK_EQ(image_loader, icon_image_loader_.get());
  SetIcon(icon_image_loader_->GetIconImage());
}

void LauncherSearchResult::OnBadgeIconImageChanged(
    LauncherSearchIconImageLoader* image_loader) {
  DCHECK_EQ(image_loader, icon_image_loader_.get());
  // No badging is required.
}

LauncherSearchResult::LauncherSearchResult(
    const std::string& item_id,
    const int discrete_value_relevance,
    Profile* profile,
    const extensions::Extension* extension,
    const scoped_refptr<LauncherSearchIconImageLoader>& icon_image_loader)
    : item_id_(item_id),
      discrete_value_relevance_(discrete_value_relevance),
      profile_(profile),
      extension_(extension),
      icon_image_loader_(icon_image_loader) {
  DCHECK(icon_image_loader_);
  Initialize();
}

void LauncherSearchResult::Initialize() {
  set_id(GetSearchResultId());
  set_relevance(static_cast<double>(discrete_value_relevance_) /
                static_cast<double>(
                    chromeos::launcher_search_provider::kMaxSearchResultScore));
  SetDetails(base::UTF8ToUTF16(extension_->name()));
  SetResultType(ResultType::kLauncher);

  icon_image_loader_->AddObserver(this);

  SetIcon(icon_image_loader_->GetIconImage());
}

std::string LauncherSearchResult::GetSearchResultId() {
  return extension_->id() + kResultIdDelimiter + item_id_;
}

}  // namespace app_list
