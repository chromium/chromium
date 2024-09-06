// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/scoped_alsa_mixer.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
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

int MixerEventCallback(snd_mixer_elem_t*, unsigned int) {
  return 0;
}

class ScopedAlsaMixerEventTest : public ::testing::Test {
 public:
  ScopedAlsaMixerEventTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_EQ(0, pipe(pipe_fds_));

    EXPECT_CALL(alsa_, MixerOpen(_, 0))
        .WillOnce(DoAll(SetArgPointee<0>(mixer_), Return(kSuccess)));
    EXPECT_CALL(alsa_, MixerAttach(mixer_, StrEq(kMixerDeviceName)))
        .WillOnce(Return(kSuccess));
    EXPECT_CALL(alsa_, MixerElementRegister(mixer_, nullptr, nullptr))
        .WillOnce(Return(kSuccess));
    EXPECT_CALL(alsa_, MixerLoad(mixer_)).WillOnce(Return(kSuccess));
    EXPECT_CALL(alsa_, MixerSelemIdMalloc(_))
        .WillOnce(DoAll(SetArgPointee<0>(mixer_selem_id_), Return(kSuccess)));
    EXPECT_CALL(alsa_, MixerSelemIdSetIndex(mixer_selem_id_, 0));
    EXPECT_CALL(alsa_,
                MixerSelemIdSetName(mixer_selem_id_, StrEq(kMixerElementName)));
    EXPECT_CALL(alsa_, MixerFindSelem(mixer_, mixer_selem_id_))
        .WillOnce(Return(element_));
    EXPECT_CALL(alsa_, MixerSelemIdFree(mixer_selem_id_));

    alsa_mixer_ = std::make_unique<ScopedAlsaMixer>(&alsa_, kMixerDeviceName,
                                                    kMixerElementName);
  }

  void TearDown() override {
    EXPECT_CALL(alsa_, MixerElemSetCallback(_, nullptr));
    EXPECT_CALL(alsa_, MixerElemSetCallbackPrivate(_, nullptr));
    EXPECT_CALL(alsa_, MixerClose(mixer_));
    alsa_mixer_.reset();
    EXPECT_EQ(0, IGNORE_EINTR(close(pipe_fds_[0])));
    EXPECT_EQ(0, IGNORE_EINTR(close(pipe_fds_[1])));
  }

  void ReadByte() {
    char buffer;
    ASSERT_TRUE(base::ReadFromFD(pipe_fds_[0], base::span_from_ref(buffer)));
  }

  void WriteByte() {
    constexpr char kByte = '!';
    ASSERT_TRUE(base::WriteFileDescriptor(pipe_fds_[1],
                                          base::byte_span_from_ref(kByte)));
  }

  base::test::TaskEnvironment task_environment_;
  int pipe_fds_[2];

  ::testing::StrictMock<::media::MockAlsaWrapper> alsa_;
  snd_mixer_t* mixer_ = reinterpret_cast<snd_mixer_t*>(0x00001111);
  snd_mixer_selem_id_t* mixer_selem_id_ =
      reinterpret_cast<snd_mixer_selem_id_t*>(0x00002222);
  snd_mixer_elem_t* element_ = reinterpret_cast<snd_mixer_elem_t*>(0x00003333);
  void* cb_private_data_ = reinterpret_cast<void*>(0x00004444);

  std::unique_ptr<ScopedAlsaMixer> alsa_mixer_ = nullptr;
};

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
  EXPECT_EQ(alsa_mixer.GetMixerForTest(), mixer);
  EXPECT_EQ(alsa_mixer.element, element);

  EXPECT_CALL(alsa, MixerElemSetCallback(element, nullptr));
  EXPECT_CALL(alsa, MixerElemSetCallbackPrivate(element, nullptr));
  EXPECT_CALL(alsa, MixerClose(mixer));
}

TEST(ScopedAlsaMixerTest, RefreshElement) {
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

  alsa_mixer.RefreshElement();

  EXPECT_CALL(alsa, MixerClose(mixer));
}

TEST(ScopedAlsaMixerTest, MixerOpenFailure) {
  ::testing::StrictMock<::media::MockAlsaWrapper> alsa;
  snd_mixer_t* mixer = reinterpret_cast<snd_mixer_t*>(0x00001111);
  base::test::MockLog mock_log;

  EXPECT_CALL(alsa, MixerOpen(_, 0))
      .WillOnce(DoAll(SetArgPointee<0>(mixer), Return(kFailure)));
  EXPECT_CALL(mock_log, Log(logging::LOGGING_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
  EXPECT_CALL(
      mock_log,
      Log(logging::LOGGING_ERROR, ::testing::EndsWith("/scoped_alsa_mixer.cc"),
          /*line=*/_,
          /*message_start=*/_, /*str=*/HasSubstr("MixerOpen error")));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.GetMixerForTest(), nullptr);
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
  EXPECT_CALL(mock_log, Log(logging::LOGGING_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
  EXPECT_CALL(
      mock_log,
      Log(logging::LOGGING_ERROR, ::testing::EndsWith("/scoped_alsa_mixer.cc"),
          /*line=*/_,
          /*message_start=*/_, HasSubstr("MixerAttach error")));
  EXPECT_CALL(alsa, MixerClose(mixer));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.GetMixerForTest(), nullptr);
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
  EXPECT_CALL(mock_log, Log(logging::LOGGING_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
  EXPECT_CALL(mock_log, Log(logging::LOGGING_ERROR,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, HasSubstr("MixerLoad error")));
  EXPECT_CALL(alsa, MixerClose(mixer));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.GetMixerForTest(), nullptr);
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
  EXPECT_CALL(mock_log, Log(logging::LOGGING_INFO,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(mock_log, Log(logging::LOGGING_ERROR,
                            ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                            /*line=*/_,
                            /*message_start=*/_, /*str=*/_));
  EXPECT_CALL(alsa, MixerSelemIdFree(mixer_selem_id));

  mock_log.StartCapturingLogs();
  ScopedAlsaMixer alsa_mixer(&alsa, kMixerDeviceName, kMixerElementName);
  EXPECT_EQ(alsa_mixer.GetMixerForTest(), mixer);
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
        EXPECT_CALL(mock_log, Log(logging::LOGGING_INFO,
                                  ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                                  /*line=*/_,
                                  /*message_start=*/_, /*str=*/_));
        EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
        EXPECT_CALL(mock_log, Log(logging::LOGGING_FATAL,
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
        EXPECT_CALL(mock_log, Log(logging::LOGGING_INFO,
                                  ::testing::EndsWith("/scoped_alsa_mixer.cc"),
                                  /*line=*/_,
                                  /*message_start=*/_, /*str=*/_));
        EXPECT_CALL(alsa, StrError(kFailure)).WillOnce(Return(""));
        EXPECT_CALL(mock_log, Log(logging::LOGGING_FATAL,
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

TEST_F(ScopedAlsaMixerEventTest, NullableCallback) {
  EXPECT_CALL(alsa_, MixerElemSetCallback(_, _)).Times(0);
  EXPECT_CALL(alsa_, MixerElemSetCallbackPrivate(_, _)).Times(0);
}

TEST_F(ScopedAlsaMixerEventTest, RealCallback) {
  base::RunLoop run_loop;

  EXPECT_CALL(alsa_, MixerElemSetCallback(element_, &MixerEventCallback));
  EXPECT_CALL(alsa_, MixerElemSetCallbackPrivate(element_, cb_private_data_));

  EXPECT_CALL(alsa_, MixerPollDescriptorsCount(mixer_)).WillOnce(Return(1));
  EXPECT_CALL(alsa_, MixerPollDescriptors(mixer_, _, 1))
      .WillOnce(testing::Invoke(
          [this](snd_mixer_t* mixer_, struct pollfd* pfds, unsigned int space) {
            for (unsigned int i = 0; i < space; ++i) {
              pfds[i].fd = pipe_fds_[0];
            }
            return space;
          }));

  EXPECT_EQ(alsa_mixer_->element, element_);
  alsa_mixer_->WatchForEvents(&MixerEventCallback, cb_private_data_);

  EXPECT_CALL(alsa_, MixerHandleEvents(mixer_))
      .WillOnce(testing::Invoke([this, &run_loop]() {
        ReadByte();
        run_loop.Quit();
        return 0;
      }));
  WriteByte();

  run_loop.Run();
}

}  // namespace media
}  // namespace chromecast
