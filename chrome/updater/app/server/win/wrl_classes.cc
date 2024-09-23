// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wrl/module.h>

#include "chrome/updater/app/server/win/com_classes.h"
#include "chrome/updater/app/server/win/com_classes_legacy.h"

namespace updater {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"

// CoCreatableClassWithFactoryEx takes three parameters, `className`, `factory`,
// and `serverName`:
// * `className` identifies the COM class from the IDL file, and not the
// corresponding C++ implementation class.
// * `factory` identifies the class factory that creates the COM object, and is
// typically Microsoft::WRL::SimpleClassFactory<>.
// * `serverName` is used below to group COM classes that need to be
// registered/unregistered together in a single process by using
// Microsoft::WRL::Module::RegisterObjects and
// Microsoft::WRL::Module::UnregisterObjects, respectively.
//
// Below, we are registering the following groups of COM objects:
// * ActiveSystem
// * ActiveUser
// * InternalSystem
// * InternalUser
CoCreatableClassWithFactoryEx(UpdaterSystemClass,
                              Microsoft::WRL::SimpleClassFactory<UpdaterImpl>,
                              ActiveSystem);
CoCreatableClassWithFactoryEx(
    GoogleUpdate3WebSystemClass,
    Microsoft::WRL::SimpleClassFactory<LegacyOnDemandImpl>,
    ActiveSystem);
CoCreatableClassWithFactoryEx(
    GoogleUpdate3WebServiceClass,
    Microsoft::WRL::SimpleClassFactory<LegacyOnDemandImpl>,
    ActiveSystem);
CoCreatableClassWithFactoryEx(
    ProcessLauncherClass,
    Microsoft::WRL::SimpleClassFactory<LegacyProcessLauncherImpl>,
    ActiveSystem);
CoCreatableClassWithFactoryEx(
    PolicyStatusSystemClass,
    Microsoft::WRL::SimpleClassFactory<PolicyStatusImpl>,
    ActiveSystem);

CoCreatableClassWithFactoryEx(UpdaterUserClass,
                              Microsoft::WRL::SimpleClassFactory<UpdaterImpl>,
                              ActiveUser);
CoCreatableClassWithFactoryEx(
    GoogleUpdate3WebUserClass,
    Microsoft::WRL::SimpleClassFactory<LegacyOnDemandImpl>,
    ActiveUser);
CoCreatableClassWithFactoryEx(
    PolicyStatusUserClass,
    Microsoft::WRL::SimpleClassFactory<PolicyStatusImpl>,
    ActiveUser);

CoCreatableClassWithFactoryEx(
    UpdaterInternalSystemClass,
    Microsoft::WRL::SimpleClassFactory<UpdaterInternalImpl>,
    InternalSystem);

CoCreatableClassWithFactoryEx(
    UpdaterInternalUserClass,
    Microsoft::WRL::SimpleClassFactory<UpdaterInternalImpl>,
    InternalUser);

#pragma clang diagnostic pop

}  // namespace updater
