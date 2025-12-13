// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AUTOFILL_STATES_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AUTOFILL_STATES_COMPONENT_INSTALLER_H_

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

void DeleteAutofillStatesComponent(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_INSTALLER_POLICIES_AUTOFILL_STATES_COMPONENT_INSTALLER_H_
