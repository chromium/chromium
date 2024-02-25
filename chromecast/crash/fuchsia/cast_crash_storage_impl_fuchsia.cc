// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/fuchsia/cast_crash_storage_impl_fuchsia.h"

#include <fuchsia/feedback/cpp/fidl.h>

#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "chromecast/crash/fuchsia/constants.h"

namespace chromecast {
namespace {

const char kLastLaunchedApp[] = "app.last-launched";
const char kCurrentApp[] = "app.current";
const char kPreviousApp[] = "app.previous";
const char kStadiaSessionId[] = "stadia-session-id";

fuchsia::feedback::Annotation MakeAnnotation(std::string_view key,
                                             std::string_view value) {
  fuchsia::feedback::Annotation annotation;
  annotation.key = std::string(key);
  annotation.value = std::string(value);
  return annotation;
}

}  // namespace

CastCrashStorageImplFuchsia::CastCrashStorageImplFuchsia(
    const sys::ServiceDirectory* incoming_directory)
    : incoming_directory_(incoming_directory) {
  DCHECK(incoming_directory_);
}

CastCrashStorageImplFuchsia::~CastCrashStorageImplFuchsia() = default;

void CastCrashStorageImplFuchsia::SetLastLaunchedApp(std::string_view app_id) {
  UpsertAnnotations({MakeAnnotation(kLastLaunchedApp, app_id)});
}

void CastCrashStorageImplFuchsia::ClearLastLaunchedApp() {
  UpsertAnnotations({MakeAnnotation(kLastLaunchedApp, std::string_view())});
}

void CastCrashStorageImplFuchsia::SetCurrentApp(std::string_view app_id) {
  UpsertAnnotations({MakeAnnotation(kCurrentApp, app_id)});
}

void CastCrashStorageImplFuchsia::ClearCurrentApp() {
  UpsertAnnotations({MakeAnnotation(kCurrentApp, std::string_view())});
}

void CastCrashStorageImplFuchsia::SetPreviousApp(std::string_view app_id) {
  UpsertAnnotations({MakeAnnotation(kPreviousApp, app_id)});
}

void CastCrashStorageImplFuchsia::ClearPreviousApp() {
  UpsertAnnotations({MakeAnnotation(kPreviousApp, std::string_view())});
}

void CastCrashStorageImplFuchsia::SetStadiaSessionId(
    std::string_view session_id) {
  UpsertAnnotations({MakeAnnotation(kStadiaSessionId, session_id)});
}

void CastCrashStorageImplFuchsia::ClearStadiaSessionId() {
  UpsertAnnotations({MakeAnnotation(kStadiaSessionId, std::string_view())});
}

void CastCrashStorageImplFuchsia::UpsertAnnotations(
    std::vector<fuchsia::feedback::Annotation> annotations) {
  fuchsia::feedback::ComponentDataRegisterPtr component_data_register;
  incoming_directory_->Connect<fuchsia::feedback::ComponentDataRegister>(
      component_data_register.NewRequest());

  fuchsia::feedback::ComponentData component_data;
  component_data.set_namespace_(crash::kCastNamespace);
  component_data.set_annotations(std::move(annotations));
  component_data_register->Upsert(std::move(component_data), []() {});
}

}  // namespace chromecast
