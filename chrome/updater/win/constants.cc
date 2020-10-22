// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/constants.h"

namespace updater {

const base::char16 kGlobalPrefix[] = L"Global\\G";

// TODO(crbug.com/1097297): need to add branding support.
const base::char16 kPrefsAccessMutex[] =
    L"{D8E4A6FE-EA7A-4D20-A8C8-B4628776A101}";

const base::char16 kRegistryValuePV[] = L"pv";
const base::char16 kRegistryValueName[] = L"name";
const base::char16 kRegistryValueInstallerProgress[] = L"InstallerProgress";

const base::char16 kWindowsServiceName[] = L"UpdaterService";

// {415FD747-D79E-42D7-93AC-1BA6E5FD4E93}
const CLSID CLSID_UpdaterServiceClass = {
    0x415FD747,
    0xD79E,
    0x42D7,
    {0x93, 0xAC, 0x1B, 0xA6, 0xE5, 0xFD, 0x4E, 0x93}};

// {1F87FE2F-D6A9-4711-9D11-8187705F8457}
const CLSID CLSID_UpdaterControlClass{
    0x1f87fe2f,
    0xd6a9,
    0x4711,
    {0x9d, 0x11, 0x81, 0x87, 0x70, 0x5f, 0x84, 0x57}};

// {22181302-A8A6-4f84-A541-E5CBFC70CC43}
const CLSID CLSID_GoogleUpdate3WebUserClass = {
    0x22181302,
    0xA8A6,
    0x4f84,
    {0xA5, 0x41, 0xE5, 0xCB, 0xFC, 0x70, 0xCC, 0x43}};

}  // namespace updater
