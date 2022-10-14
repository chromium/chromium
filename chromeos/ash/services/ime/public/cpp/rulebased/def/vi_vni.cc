// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/vi_vni.h"

#include <iterator>

namespace vi_vni {

const char* kId = "vi_vni";
bool kIs102 = false;
const char* kTransforms[] = {
    "d\u001d?9",
    "\u0111",
    "D\u001d?9",
    "\u0110",
    "(\\w*)a\u001d?6",
    "\\1\u00e2",
    "(\\w*)e\u001d?6",
    "\\1\u00ea",
    "(\\w*)o\u001d?6",
    "\\1\u00f4",
    "(\\w*)a\u001d?8",
    "\\1\u0103",
    "(\\w*)o\u001d?7",
    "\\1\u01a1",
    "(\\w*)u\u001d?7",
    "\\1\u01b0",
    "(\\w*)A\u001d?6",
    "\\1\u00c2",
    "(\\w*)E\u001d?6",
    "\\1\u00ca",
    "(\\w*)O\u001d?6",
    "\\1\u00d4",
    "(\\w*)A\u001d?8",
    "\\1\u0102",
    "(\\w*)O\u001d?7",
    "\\1\u01a0",
    "(\\w*)U\u001d?7",
    "\\1\u01af",
    "(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    "bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?2",
    "\\1\u0300\\3",
    "(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    "bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?1",
    "\\1\u0301\\3",
    "(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    "bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?3",
    "\\1\u0309\\3",
    "(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    "bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?4",
    "\\1\u0303\\3",
    "(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY])([aeiouyAEIOUY]?|["
    "bcdghklmnpqtvBCDGHKLMNPQTV]+)\u001d?5",
    "\\1\u0323\\3",
    "(["
    "\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    "])\u001d?([a-zA-Z]*)2",
    "\\1\u0300\\2",
    "(["
    "\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    "])\u001d?([a-zA-Z]*)1",
    "\\1\u0301\\2",
    "(["
    "\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    "])\u001d?([a-zA-Z]*)3",
    "\\1\u0309\\2",
    "(["
    "\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    "])\u001d?([a-zA-Z]*)4",
    "\\1\u0303\\2",
    "(["
    "\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    "])\u001d?([a-zA-Z]*)5",
    "\\1\u0323\\2",
    "(([qQ][uU]|[gG][iI])?[aeiouyAEIOUY]+)([\u0300\u0301\u0303\u0309\u0323])"
    "\u001d?([aeiouyAEIOUY])\u001d?([a-eg-ik-qtuvyA-EG-IK-QTUVY])",
    "\\1\\4\\3\\5",
    "([\u0300\u0301\u0303\u0309\u0323])(["
    "\u0103\u00e2\u00ea\u00f4\u01a1\u01b0\u0102\u00c2\u00ca\u00d4\u01a0\u01af"
    "])\u001d?([a-zA-Z])",
    "\\2\\1\\3"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace vi_vni
