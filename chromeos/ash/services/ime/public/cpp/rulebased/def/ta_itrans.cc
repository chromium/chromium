// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/ta_itrans.h"

#include <iterator>

namespace ta_itrans {

const char* kId = "ta_itrans";
bool kIs102 = false;
const char* kTransforms[] = {"k",    "\u0b95\u0bcd",
                             "g",    "\u0b95\u0bcd",
                             "~N",   "\u0b99\u0bcd",
                             "N\\^", "\u0b99\u0bcd",
                             "ch",   "\u0b9a\u0bcd",
                             "j",    "\u0b9c\u0bcd",
                             "~n",   "\u0b9e\u0bcd",
                             "JN",   "\u0b9e\u0bcd",
                             "T",    "\u0b9f\u0bcd",
                             "Th",   "\u0b9f\u0bcd",
                             "N",    "\u0ba3\u0bcd",
                             "t",    "\u0ba4\u0bcd",
                             "th",   "\u0ba4\u0bcd",
                             "n",    "\u0ba8\u0bcd",
                             "\\^n", "\u0ba9\u0bcd",
                             "nh",   "\u0ba9",
                             "p",    "\u0baa\u0bcd",
                             "b",    "\u0baa\u0bcd",
                             "m",    "\u0bae\u0bcd",
                             "y",    "\u0baf\u0bcd",
                             "r",    "\u0bb0\u0bcd",
                             "R",    "\u0bb1\u0bcd",
                             "rh",   "\u0bb1",
                             "l",    "\u0bb2\u0bcd",
                             "L",    "\u0bb3\u0bcd",
                             "ld",   "\u0bb3\u0bcd",
                             "J",    "\u0bb4\u0bcd",
                             "z",    "\u0bb4\u0bcd",
                             "v",    "\u0bb5\u0bcd",
                             "w",    "\u0bb5\u0bcd",
                             "Sh",   "\u0bb7\u0bcd",
                             "shh",  "\u0bb7",
                             "s",    "\u0bb8\u0bcd",
                             "h",    "\u0bb9\u0bcd",
                             "GY",   "\u0b9c\u0bcd\u0b9e",
                             "dny",  "\u0b9c\u0bcd\u0b9e",
                             "x",    "\u0b95\u0bcd\u0bb7\u0bcd",
                             "ksh",  "\u0b95\u0bcd\u0bb7\u0bcd",
                             "a",    "\u0b85",
                             "aa",   "\u0b86",
                             "A",    "\u0b86",
                             "i",    "\u0b87",
                             "ii",   "\u0b88",
                             "I",    "\u0b88",
                             "u",    "\u0b89",
                             "uu",   "\u0b8a",
                             "U",    "\u0b8a",
                             "e",    "\u0b8e",
                             "E",    "\u0b8f",
                             "ee",   "\u0b8f",
                             "ai",   "\u0b90",
                             "o",    "\u0b92",
                             "O",    "\u0b93",
                             "oo",   "\u0b93",
                             "au",   "\u0b94",
                             "\\.n", "\u0b82",
                             "M",    "\u0b82",
                             "q",    "\u0b83",
                             "H",    "\u0b83",
                             "\\.h", "\u0bcd",
                             "0",    "\u0be6",
                             "1",    "\u0be7",
                             "2",    "\u0be8",
                             "3",    "\u0be9",
                             "4",    "\u0bea",
                             "5",    "\u0beb",
                             "6",    "\u0bec",
                             "7",    "\u0bed",
                             "8",    "\u0bee",
                             "9",    "\u0bef",
                             "#",    "\u0bcd",
                             "\\$",  "\u0bb0",
                             "\\^",  "\u0ba4\u0bcd"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = "[\\^lrshkdnJNtTaeiouAEIOU]|sh|ks|dn";

}  // namespace ta_itrans
