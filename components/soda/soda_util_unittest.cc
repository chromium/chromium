// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_util.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#endif

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace speech {

TEST(SodaUtilTest, IsOnDeviceSpeechRecognitionSupported) {
#if BUILDFLAG(IS_CHROMEOS)
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        ash::features::kOnDeviceSpeechRecognition);
    EXPECT_TRUE(IsOnDeviceSpeechRecognitionSupported());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        ash::features::kOnDeviceSpeechRecognition);
    EXPECT_FALSE(IsOnDeviceSpeechRecognitionSupported());
  }
#elif BUILDFLAG(IS_LINUX)
#if defined(ARCH_CPU_X86_FAMILY)
  EXPECT_EQ(IsOnDeviceSpeechRecognitionSupported(), base::CPU().has_avx());
#else
  EXPECT_FALSE(IsOnDeviceSpeechRecognitionSupported());
#endif
#elif BUILDFLAG(IS_MAC)
#if defined(ARCH_CPU_X86_FAMILY)
  EXPECT_EQ(IsOnDeviceSpeechRecognitionSupported(), base::CPU().has_avx());
#elif defined(ARCH_CPU_ARM_FAMILY)
  EXPECT_TRUE(IsOnDeviceSpeechRecognitionSupported());
#else
  EXPECT_FALSE(IsOnDeviceSpeechRecognitionSupported());
#endif
#elif BUILDFLAG(IS_WIN)
#if defined(ARCH_CPU_X86_FAMILY)
  EXPECT_EQ(IsOnDeviceSpeechRecognitionSupported(), base::CPU().has_avx());
#else
  EXPECT_TRUE(IsOnDeviceSpeechRecognitionSupported());
#endif
#else
  EXPECT_FALSE(IsOnDeviceSpeechRecognitionSupported());
#endif
}

}  // namespace speech
