// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_CRL_SET_REMOVER_H_
#define COMPONENTS_COMPONENT_UPDATER_CRL_SET_REMOVER_H_

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

// TODO(waffles): Remove in M66 or later.
void DeleteLegacyCRLSet(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_CRL_SET_REMOVER_H_
