// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SPEECH_SODA_SODA_TEST_PATHS_H_
#define CHROME_SERVICES_SPEECH_SODA_SODA_TEST_PATHS_H_

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace soda {

#if BUILDFLAG(IS_MAC)

constexpr base::FilePath::CharType kSodaResourcePath[] =
    FILE_PATH_LITERAL("third_party/soda-mac64/resources");

constexpr base::FilePath::CharType kSodaTestBinaryRelativePath[] =
    FILE_PATH_LITERAL("libsoda_for_testing.so");

#elif BUILDFLAG(IS_WIN) && defined(ARCH_CPU_64_BITS)

constexpr base::FilePath::CharType kSodaResourcePath[] =
    FILE_PATH_LITERAL("third_party/soda-win64/resources");

constexpr base::FilePath::CharType kSodaTestBinaryRelativePath[] =
    FILE_PATH_LITERAL("SODA_for_testing.dll");

#elif BUILDFLAG(IS_WIN) && defined(ARCH_CPU_32_BITS)

constexpr base::FilePath::CharType kSodaResourcePath[] =
    FILE_PATH_LITERAL("third_party/soda-win32/resources");

constexpr base::FilePath::CharType kSodaTestBinaryRelativePath[] =
    FILE_PATH_LITERAL("SODA_for_testing.dll");

#elif BUILDFLAG(IS_LINUX)

constexpr base::FilePath::CharType kSodaResourcePath[] =
    FILE_PATH_LITERAL("third_party/soda/resources");

constexpr base::FilePath::CharType kSodaTestBinaryRelativePath[] =
    FILE_PATH_LITERAL("libsoda_for_testing.so");

#endif

constexpr base::FilePath::CharType kSodaLanguagePackRelativePath[] =
    FILE_PATH_LITERAL("en_us");

constexpr base::FilePath::CharType kSodaTestAudioRelativePath[] =
    FILE_PATH_LITERAL("hey_google.wav");

}  // namespace soda

#endif  // CHROME_SERVICES_SPEECH_SODA_SODA_TEST_PATHS_H_
