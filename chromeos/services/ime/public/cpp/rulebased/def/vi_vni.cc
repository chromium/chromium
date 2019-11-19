// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/def/vi_vni.h"

#include "base/stl_util.h"

namespace vi_vni {

const char* kId = "vi_vni";
bool kIs102 = false;
const char* kTransforms[] = {
    u8"d\u001d?9",
    u8"\u0111",
    u8"D\u001d?9",
    u8"\u0110",
    u8"(\\w*)a\u001d?6",
    u8"\\1\u00e2",
    u8"(\\w*)e\u001d?6",
    u8"\\1\u00ea",
    u8"(\\w*)o\u001d?6",
    u8"\\1\u00f4",
    u8"(\\w*)a\u001d?8",
    u8"\\1\u0103",
    u8"(\\w*)o\u001d?7",
    u8"\\1\u01a1",
    u8"(\\w*)u\u001d?7",
    u8"\\1\u01b0",
    u8"(\\w*)A\u001d?6",
    u8"\\1\u00c2",
    u8"(\\w*)E\u001d?6",
    u8"\\1\u00ca",
    u8"(\\w*)O\u001d?6",
    u8"\\1\u00d4",
    u8"(\\w*)A\u001d?8",
    u8"\\1\u0102",
    u8"(\\w*)O\u001d?7",
    u8"\\1\u01a0",
    u8"(\\w*)U\u001d?7",
    u8"\\1\u01af",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?2",
    u8"\\1\u0300\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?1",
    u8"\\1\u0301\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?3",
    u8"\\1\u0309\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?4",
    u8"\\1\u0303\\3",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    u8"bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?5",
    u8"\\1\u0323\\3",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)2",
    u8"\\1\u0300\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)1",
    u8"\\1\u0301\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)3",
    u8"\\1\u0309\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)4",
    u8"\\1\u0303\\2",
    u8"(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z]*)5",
    u8"\\1\u0323\\2",
    u8"(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY]+)([\u0300\u0301\u0303\u0309\u0323])"
    u8"\u001d?([aeiouyAEIOUY])\u001d?([a-eg-ik-qtuvyA-EG-IK-QTUVY])",
    u8"\\1\\4\\3\\5",
    u8"([\u0300\u0301\u0303\u0309\u0323])(["
    u8"\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    u8"])\u001d?([a-zA-Z])",
    u8"\\2\\1\\3"};
const unsigned int kTransformsLen = base::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace vi_vni
