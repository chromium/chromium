// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_CLIENT_VERSION_INFO_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_CLIENT_VERSION_INFO_H_

// The version is used to verify if the group data provided by the server
// is supported in client current version of the code. Update the version
// number for any new feature added to the proto, and add a log of the new
// changes in the current version in this file.
// Version 1 supports collaboration group features create, join and manage.
enum class ClientVersionInfo {
    CURRENT_VERSION = 1
};

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_CLIENT_VERSION_INFO_H_
