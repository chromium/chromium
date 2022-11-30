// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/common/engine_resources.h"

#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"

namespace chrome_cleaner {

void InitializePUPDataWithCatalog(Engine::Name engine) {
  PUPData::InitializePUPData({&TestUwSCatalog::GetInstance()});
}

int GetProtectedFilesDigestResourceId() {
  return 0;
}

std::unordered_map<std::wstring, int> GetEmbeddedLibraryResourceIds(
    Engine::Name engine) {
  return {};
}

int GetLibrariesDigestResourcesId(Engine::Name engine) {
  return 0;
}

std::wstring GetTestStubFileName(Engine::Name engine) {
  return std::wstring();
}

std::set<std::wstring> GetLibrariesToLoad(Engine::Name engine) {
  return {};
}

std::unordered_map<std::wstring, std::wstring> GetLibraryTestReplacements(
    Engine::Name engine) {
  return {};
}

std::vector<std::wstring> GetDLLNames(Engine::Name engine) {
  return {};
}

}  // namespace chrome_cleaner
