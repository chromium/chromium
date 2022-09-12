// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/scoped_alsa_mixer.h"

#include "base/logging.h"
#include "base/test/mock_log.h"
#include "media/audio/alsa/mock_alsa_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;

constexpr char kMixerDeviceName[] = "test-device";
constexpr char kMixerElementName[] = "test-element";

const int kSuccess = 0;
const int kFailure = -1;
}  // namespace

TEST(ScopedAlsaMixerTest, NormalLifeCycle) {
  ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  snd_mixer_selem_id_t* mixer_selem_id =
      reinterpret_cast<snd_mixer_selem_id_t*>(0x00002222);
  snd_mixer_elem_t* element = reinterpret_cast<snd_mixer_elem_t*>(0x00003333);

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerElementRegister(mixer, nullptr, nullptr))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerLoad(mixer)).WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerSelemIdMalloc(_))
      .WillOnce(DoAll(SetArgPointee<0>(mixer_selem_id), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerSelemIdSetIndex(mixer_selem_id, 0));
  EXPECT_CALL(alsa,
              MixerSelemIdSetName(mixer_selem_id, StrEq(kMixerElementName)));
  EXPECT_CALL(alsa, MixerFindSelem(mixer, mixer_selem_id))
      .WillOnce(Return(element));
  EXPECT_CALL(alsa, MixerSelemIdFree(mixer_selem_id));

  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.mixer, mixer);
  EXPECT_EQ(alsa_mixer.element, element);

  EXPECT_CALL(alsa, MixerClose(mixer));
}

TEST(ScopedAlsaMixerTest, Refresh) {
  ::testing::NiceMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  snd_mixer_selem_id_t* mixer_selem_id =
      reinterpret_cast<snd_mixer_selem_id_t*>(0x00002222);
  snd_mixer_elem_t* element = reinterpret_cast<snd_mixer_elem_t*>(0x00003333);
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerElementRegister(mixer, nullptr, nullptr))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerLoad(mixer)).WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerSelemIdMalloc(_))
      .WillOnce(DoAll(SetArgPointee<0>(mixer_selem_id), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerSelemIdSetIndex(mixer_selem_id, 0));
  EXPECT_CALL(alsa,
              MixerSelemIdSetName(mixer_selem_id, StrEq(kMixerElementName)));
  EXPECT_CALL(alsa, MixerFindSelem(mixer, mixer_selem_id))
      .WillOnce(Return(element));
  EXPECT_CALL(alsa, MixerSelemIdFree(mixer_selem_id));

  alsa_mixer.Refresh();

  EXPECT_CALL(alsa, MixerClose(mixer));
}

TEST(ScopedAlsaMixerTest, MixerOpenFailure) {
  ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  base::test::MockLog mock_log;

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kFailure)));
  EXPECT_CALL(mock_log, Log(logging::LOG_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
  EXPECT_CALL(
      mock_log,
      Log(logging::LOG_ERROR, ::testing::EndsWith("/scoped_alsa_mixer.cc"),
          /*line=*/_,
          /*message_start=*/_, /*str=*/HasSubstr("MixerOpen error")));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.mixer, nullptr);
  EXPECT_EQ(alsa_mixer.element, nullptr);
  mock_log.StopCapturingLogs();
}

TEST(ScopedAlsaMixerTest, MixerAttachFailure) {
  ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  base::test::MockLog mock_log;

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
      .WillOnce(Return(kFailure));
  EXPECT_CALL(mock_log, Log(logging::LOG_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
  EXPECT_CALL(
      mock_log,
      Log(logging::LOG_ERROR, ::testing::EndsWith("/scoped_alsa_mixer.cc"),
          /*line=*/_,
          /*message_start=*/_, HasSubstr("MixerAttach error")));
  EXPECT_CALL(alsa, MixerClose(mixer));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.mixer, nullptr);
  EXPECT_EQ(alsa_mixer.element, nullptr);
  mock_log.StopCapturingLogs();
}

TEST(ScopedAlsaMixerTest, MixerLoadFailure) {
  ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  base::test::MockLog mock_log;

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerElementRegister(mixer, nullptr, nullptr))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerLoad(mixer)).WillOnce(Return(kFailure));
  EXPECT_CALL(mock_log, Log(logging::LOG_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
  EXPECT_CALL(mock_log, Log(logging::LOG_ERROR,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, HasSubstr("MixerLoad error")));
  EXPECT_CALL(alsa, MixerClose(mixer));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.mixer, nullptr);
  EXPECT_EQ(alsa_mixer.element, nullptr);
  mock_log.StopCapturingLogs();
}

TEST(ScopedAlsaMixerTest, MixerFindSelemFailure) {
  ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  snd_mixer_selem_id_t* mixer_selem_id =
      reinterpret_cast<snd_mixer_selem_id_t*>(0x00002222);
  base::test::MockLog mock_log;

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerElementRegister(mixer, nullptr, nullptr))
      .WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerLoad(mixer)).WillOnce(Return(kSuccess));
  EXPECT_CALL(alsa, MixerSelemIdMalloc(_))
      .WillOnce(DoAll(SetArgPointee<0>(mixer_selem_id), Return(kSuccess)));
  EXPECT_CALL(alsa, MixerSelemIdSetIndex(mixer_selem_id, 0));
  EXPECT_CALL(alsa,
              MixerSelemIdSetName(mixer_selem_id, StrEq(kMixerElementName)));
  EXPECT_CALL(alsa, MixerFindSelem(mixer, mixer_selem_id))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_log, Log(logging::LOG_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(mock_log, Log(logging::LOG_ERROR,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, MixerSelemIdFree(mixer_selem_id));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.mixer, mixer);
  EXPECT_EQ(alsa_mixer.element, nullptr);
  mock_log.StopCapturingLogs();

  EXPECT_CALL(alsa, MixerClose(mixer));
}

TEST(ScopedAlsaMixerDeathTest, MixerElementRegisterFailure) {
  EXPECT_DEATH(
      {
        ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
        snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
        base::test::MockLog mock_log;

        EXPECT_CALL(alsa, MixerOpen(_, 0))
            .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
        EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
            .WillOnce(Return(kSuccess));
        EXPECT_CALL(alsa, MixerElementRegister(mixer, nullptr, nullptr))
            .WillOnce(Return(kFailure));
        EXPECT_CALL(mock_log, Log(logging::LOG_INFO,
                                  ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                                  /*line=*/_,
                                  /*message_start=*/_, /*str=*/_));
        EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
        EXPECT_CALL(mock_log, Log(logging::LOG_FATAL,
                                  ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                                  /*line=*/_,
                                  /*message_start=*/_,
                                  HasSubstr("MixerElementRegister error")));

        mock_log.StartCapturingLogs();
        ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
        mock_log.StopCapturingLogs();
      },
      _);
}

TEST(ScopedAlsaMixerDeathTest, MixerSelemIdMallocFailure) {
  EXPECT_DEATH(
      {
        ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
        snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
        snd_mixer_selem_id_t* mixer_selem_id =
            reinterpret_cast<snd_mixer_selem_id_t*>(0x00002222);
        base::test::MockLog mock_log;

        EXPECT_CALL(alsa, MixerOpen(_, 0))
            .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kSuccess)));
        EXPECT_CALL(alsa, MixerAttach(mixer, StrEq(kMixerDeviceName)))
            .WillOnce(Return(kSuccess));
        EXPECT_CALL(alsa, MixerElementRegister(mixer, nullptr, nullptr))
            .WillOnce(Return(kSuccess));
        EXPECT_CALL(alsa, MixerLoad(mixer)).WillOnce(Return(kSuccess));
        EXPECT_CALL(alsa, MixerSelemIdMalloc(_))
            .WillOnce(
                DoAll(SetArgPointee<0>(mixer_selem_id), Return(kFailure)));
        EXPECT_CALL(mock_log, Log(logging::LOG_INFO,
                                  ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                                  /*line=*/_,
                                  /*message_start=*/_, /*str=*/_));
        EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
        EXPECT_CALL(mock_log, Log(logging::LOG_FATAL,
                                  ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                                  /*line=*/_,
                                  /*message_start=*/_,
                                  HasSubstr("MixerSelemIdMalloc error")));

        mock_log.StartCapturingLogs();
        ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
        mock_log.StopCapturingLogs();
      },
      _);
}

}  // namespace media
}  // namespace chromecast
