// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_RESOURCES_H_
#define CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_RESOURCES_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"

namespace chrome_cleaner {

// Initializes PUPData with the engine's catalog.
void InitializePUPDataWithCatalog(Engine::Name engine);

// Functions to find resource ids for the engine.

// Returns ID of the "TEXT" resource that contains serialized FileDigests
// message (see file_digest.proto) of protected files. Returns zero if not
// available.
int GetProtectedFilesDigestResourceId();

// Returns ID of the "TEXT" resource that contains seralized UwEMatcher
// messages (see uwe_matcher.proto). Returns zero if not available.
//
// Each engine has its own UwEMatcher resource because the resource will
// contain engine-specific UwS ID's.
int GetUwEMatchersResourceID();

// Returns IDs of the "LIBRARY" resources for |engine| that are embedded into
// the executable.
std::unordered_map<base::string16, int> GetEmbeddedLibraryResourceIds(
    Engine::Name engine);

// Returns ID of the "TEXT" resource that contains serialized FileDigests
// message (see file_digest.proto) of |engine| libraries.
int GetLibrariesDigestResourcesId(Engine::Name engine);

// Functions to find files used by the engine.

// Returns the name of the stub DLL to be used in testing and an empty string
// if there are none.
base::string16 GetTestStubFileName(Engine::Name engine);

// Returns set of libraries that should be loaded for |engine|.
std::set<base::string16> GetLibrariesToLoad(Engine::Name engine);

// Returns a map of test library replacements. If during delayed loading a
// library is missing, it can be replaced by the corresponding library from the
// map. To be used in test builds only.
std::unordered_map<base::string16, base::string16> GetLibraryTestReplacements(
    Engine::Name engine);

// Returns the names of the dlls that are used by the engine
// when there are no test replacements.
std::vector<base::string16> GetDLLNames(Engine::Name engine);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_COMMON_ENGINE_RESOURCES_H_
