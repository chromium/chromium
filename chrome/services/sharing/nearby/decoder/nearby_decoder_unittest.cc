// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/decoder/nearby_decoder.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/services/sharing/public/cpp/conversions.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sharing {

namespace {

const char kDeviceName[] = "deviceName";
// Salt for advertisement.
const std::vector<uint8_t> kSalt(Advertisement::kSaltSize, 0);
// Key for encrypting personal info metadata.
static const std::vector<uint8_t> kEncryptedMetadataKey(
    Advertisement::kMetadataEncryptionKeyHashByteSize,
    0);
const nearby_share::mojom::ShareTargetType kDeviceType =
    nearby_share::mojom::ShareTargetType::kPhone;

void ExpectEquals(const Advertisement& self,
                  const mojom::AdvertisementPtr& other) {
  EXPECT_EQ(self.device_type(), other->device_type);
  EXPECT_EQ(self.device_name(), other->device_name);
  EXPECT_EQ(self.salt(), other->salt);
  EXPECT_EQ(self.encrypted_metadata_key(), other->encrypted_metadata_key);
}

void ExpectFrameContainsIntroduction(
    const mojom::FramePtr& frame,
    const std::vector<sharing::nearby::FileMetadata>& file_metadata,
    const std::vector<sharing::nearby::TextMetadata>& text_metadata,
    const std::string& required_package,
    const std::vector<sharing::nearby::WifiCredentialsMetadata>&
        wifi_credentials_metadata) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->is_v1());
  EXPECT_TRUE(frame->get_v1()->is_introduction());
  mojom::IntroductionFramePtr& intro = frame->get_v1()->get_introduction();

  // Verify contents of FileMetadata vector.
  size_t file_size = intro->file_metadata.size();
  ASSERT_EQ(file_metadata.size(), file_size);
  for (size_t i = 0; i < file_size; i++) {
    mojom::FileMetadataPtr& file = intro->file_metadata[i];
    EXPECT_EQ(file_metadata[i].name(), file->name);
    EXPECT_EQ(file_metadata[i].type(), ConvertFileMetadataType(file->type));
    EXPECT_EQ(file_metadata[i].payload_id(), file->payload_id);
    EXPECT_EQ(base::checked_cast<uint64_t>(file_metadata[i].size()),
              file->size);
    EXPECT_EQ(file_metadata[i].mime_type(), file->mime_type);
    EXPECT_EQ(file_metadata[i].id(), file->id);
  }

  // Verify contents of TextMetadata vector.
  size_t text_size = intro->text_metadata.size();
  ASSERT_EQ(text_size, text_metadata.size());
  for (size_t i = 0; i < text_size; i++) {
    mojom::TextMetadataPtr& text = intro->text_metadata[i];
    EXPECT_EQ(text_metadata[i].text_title(), text->text_title);
    EXPECT_EQ(text_metadata[i].type(), ConvertTextMetadataType(text->type));
    EXPECT_EQ(text_metadata[i].payload_id(), text->payload_id);
    EXPECT_EQ(base::checked_cast<uint64_t>(text_metadata[i].size()),
              text->size);
    EXPECT_EQ(text_metadata[i].id(), text->id);
  }

  EXPECT_EQ(required_package, intro->required_package);

  // Verify contents of WifiCredentialsMetadata vector.
  size_t wifi_size = intro->wifi_credentials_metadata.size();
  ASSERT_EQ(wifi_credentials_metadata.size(), wifi_size);
  for (size_t i = 0; i < wifi_size; i++) {
    mojom::WifiCredentialsMetadataPtr& wifi =
        intro->wifi_credentials_metadata[i];
    EXPECT_EQ(wifi_credentials_metadata[i].ssid(), wifi->ssid);
    EXPECT_EQ(wifi_credentials_metadata[i].security_type(),
              ConvertWifiCredentialsMetadataType(wifi->security_type));
    EXPECT_EQ(wifi_credentials_metadata[i].payload_id(), wifi->payload_id);
    EXPECT_EQ(wifi_credentials_metadata[i].id(), wifi->id);
  }
}

std::unique_ptr<sharing::nearby::Frame> BuildResponseFrame(
    sharing::nearby::ConnectionResponseFrame_Status status) {
  std::unique_ptr<sharing::nearby::Frame> frame =
      std::make_unique<sharing::nearby::Frame>();
  frame->set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame->mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_RESPONSE);
  sharing::nearby::ConnectionResponseFrame* response =
      v1frame->mutable_connection_response();
  response->set_status(status);
  return frame;
}

void ExpectFrameContainsResponse(
    const mojom::FramePtr& frame,
    mojom::ConnectionResponseFrame::Status status) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->is_v1());
  EXPECT_TRUE(frame->get_v1()->is_connection_response());
  EXPECT_EQ(status, frame->get_v1()->get_connection_response()->status);
}

void ExpectFrameContainsPairedKeyEncryption(const mojom::FramePtr& frame,
                                            const std::string& signed_data,
                                            const std::string& secret_id_hash) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->is_v1());
  EXPECT_TRUE(frame->get_v1()->is_paired_key_encryption());
  EXPECT_EQ(std::vector<uint8_t>(signed_data.begin(), signed_data.end()),
            frame->get_v1()->get_paired_key_encryption()->signed_data);
  EXPECT_EQ(std::vector<uint8_t>(secret_id_hash.begin(), secret_id_hash.end()),
            frame->get_v1()->get_paired_key_encryption()->secret_id_hash);
}

void ExpectFrameContainsCancelFrame(const mojom::FramePtr& frame) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->is_v1());
  EXPECT_TRUE(frame->get_v1()->is_cancel_frame());
}

std::unique_ptr<sharing::nearby::Frame> BuildPairedKeyResultFrame(
    sharing::nearby::PairedKeyResultFrame_Status status) {
  std::unique_ptr<sharing::nearby::Frame> frame =
      std::make_unique<sharing::nearby::Frame>();
  frame->set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame->mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_PAIRED_KEY_RESULT);
  sharing::nearby::PairedKeyResultFrame* paired_key =
      v1frame->mutable_paired_key_result();
  paired_key->set_status(status);
  return frame;
}

void ExpectFrameContainsPairedKeyResult(
    const mojom::FramePtr& frame,
    mojom::PairedKeyResultFrame::Status status) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->is_v1());
  EXPECT_TRUE(frame->get_v1()->is_paired_key_result());
  EXPECT_EQ(status, frame->get_v1()->get_paired_key_result()->status);
}

std::vector<uint8_t> StringToVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

void ExpectFrameContainsCertificateInfo(
    const mojom::FramePtr& frame,
    const std::vector<sharing::nearby::PublicCertificate>& certs) {
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame->is_v1());
  EXPECT_TRUE(frame->get_v1()->is_certificate_info());

  size_t size =
      frame->get_v1()->get_certificate_info()->public_certificate.size();
  ASSERT_EQ(size, certs.size());
  for (size_t i = 0; i < size; i++) {
    mojom::PublicCertificatePtr& cert =
        frame->get_v1()->get_certificate_info()->public_certificate[i];

    EXPECT_EQ(StringToVector(certs[i].secret_id()), cert->secret_id);
    EXPECT_EQ(StringToVector(certs[i].authenticity_key()),
              cert->authenticity_key);
    EXPECT_EQ(StringToVector(certs[i].public_key()), cert->public_key);
    EXPECT_EQ(
        base::Time::UnixEpoch() + base::Milliseconds(certs[i].start_time()),
        cert->start_time);
    EXPECT_EQ(base::Time::UnixEpoch() + base::Milliseconds(certs[i].end_time()),
              cert->end_time);
    EXPECT_EQ(StringToVector(certs[i].encrypted_metadata_bytes()),
              cert->encrypted_metadata_bytes);
    EXPECT_EQ(StringToVector(certs[i].metadata_encryption_key_tag()),
              cert->metadata_encryption_key_tag);
  }
}

}  // namespace

class NearbySharingDecoderTest : public testing::Test {
 public:
  NearbySharingDecoderTest() {
    decoder_ = std::make_unique<NearbySharingDecoder>(
        remote_.BindNewPipeAndPassReceiver(),
        /*on_disconnect=*/base::DoNothing());
  }

  NearbySharingDecoder* decoder() const { return decoder_.get(); }

  void ExpectNullFrame(const sharing::nearby::Frame& frame) {
    std::vector<uint8_t> data;
    int size = frame.ByteSize();
    if (size > 0) {
      data.resize(size);
      ASSERT_TRUE(frame.SerializeToArray(&data[0], size));
    }
    base::RunLoop run_loop;
    auto callback =
        base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
          EXPECT_FALSE(answer);
          run_loop.Quit();
        });
    decoder()->DecodeFrame(std::move(data), std::move(callback));
    run_loop.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::Remote<mojom::NearbySharingDecoder> remote_;
  std::unique_ptr<NearbySharingDecoder> decoder_;
};

TEST_F(NearbySharingDecoderTest, V1VisibleToEveryoneAdvertisementDecoding) {
  const std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(kSalt, kEncryptedMetadataKey,
                                          kDeviceType, kDeviceName);
  std::vector<uint8_t> v1EndpointInfo = {
      2, 0, 0, 0,  0,   0,   0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 100, 101, 118, 105, 99, 101, 78, 97, 109, 101};
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&](const mojom::AdvertisementPtr answer) {
        ExpectEquals(*advertisement, answer);
        run_loop.Quit();
      });
  decoder()->DecodeAdvertisement(v1EndpointInfo, std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, InvalidDeviceNameAdvertisementDecoding) {
  std::vector<uint8_t> v1EndpointInfo = {
      2, 0, 0, 0,  0,   0,  0,   0,   0,  0,   0,  0,  0,   0,
      0, 0, 0, 10, 226, 40, 161, 105, 99, 101, 78, 97, 109, 101,
  };
  auto callback = base::BindLambdaForTesting(
      [&](const mojom::AdvertisementPtr answer) { EXPECT_FALSE(answer); });
  decoder()->DecodeAdvertisement(v1EndpointInfo, std::move(callback));
}

TEST_F(NearbySharingDecoderTest, MissingFrameVersionDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, MissingV1FrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, V1FrameMissingTypeDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->mutable_introduction();

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, V1FrameMissingIntroductionFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_INTRODUCTION);

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, IntroductionFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_INTRODUCTION);
  sharing::nearby::IntroductionFrame* intro = v1frame->mutable_introduction();

  // Build several FileMetadatas.
  int num_files = sharing::nearby::FileMetadata_Type_Type_MAX;
  std::vector<sharing::nearby::FileMetadata> files;
  files.reserve(num_files);

  for (int i = 0; i < num_files; i++) {
    sharing::nearby::FileMetadata* file = intro->add_file_metadata();
    file->set_name("file " + base::NumberToString(i));
    file->set_type(static_cast<sharing::nearby::FileMetadata_Type>(i));
    file->set_payload_id(i);
    file->set_size(i);
    file->set_mime_type("mime " + base::NumberToString(i));
    file->set_id(i);
    files.push_back(*file);
  }

  // Build several TextMetadatas.
  int num_texts = sharing::nearby::TextMetadata_Type_Type_MAX;
  std::vector<sharing::nearby::TextMetadata> texts;
  texts.reserve(num_texts);

  for (int i = 0; i < num_texts; i++) {
    sharing::nearby::TextMetadata* text = intro->add_text_metadata();
    text->set_text_title("title " + base::NumberToString(i));
    text->set_type(static_cast<sharing::nearby::TextMetadata_Type>(i));
    text->set_payload_id(i);
    text->set_size(i);
    text->set_id(i);
    texts.push_back(*text);
  }

  // Set required package.
  std::string required_package = "foo package";
  intro->set_required_package(required_package);

  // Build several WifiCredentialsMetadatas.
  int num_wifis =
      sharing::nearby::WifiCredentialsMetadata_SecurityType_SecurityType_MAX;
  std::vector<sharing::nearby::WifiCredentialsMetadata> wifis;
  wifis.reserve(num_wifis);

  for (int i = 0; i < num_wifis; i++) {
    sharing::nearby::WifiCredentialsMetadata* wifi =
        intro->add_wifi_credentials_metadata();
    wifi->set_ssid("ssid " + base::NumberToString(i));
    wifi->set_security_type(
        static_cast<sharing::nearby::WifiCredentialsMetadata_SecurityType>(i));
    wifi->set_payload_id(i);
    wifi->set_id(i);
    wifis.push_back(*wifi);
  }

  std::vector<uint8_t> data;
  data.resize(frame.ByteSize());
  ASSERT_TRUE(frame.SerializeToArray(&data[0], frame.ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsIntroduction(answer, files, texts, required_package,
                                    wifis);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, V1FrameMissingResponseFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_RESPONSE);

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, ResponseFrameAcceptDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildResponseFrame(
      sharing::nearby::ConnectionResponseFrame_Status_ACCEPT);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsResponse(
        answer, mojom::ConnectionResponseFrame::Status::kAccept);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, ResponseFrameRejectDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildResponseFrame(
      sharing::nearby::ConnectionResponseFrame_Status_REJECT);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsResponse(
        answer, mojom::ConnectionResponseFrame::Status::kReject);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, ResponseFrameNotEnoughSpaceDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildResponseFrame(
      sharing::nearby::ConnectionResponseFrame_Status_NOT_ENOUGH_SPACE);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsResponse(
        answer, mojom::ConnectionResponseFrame::Status::kNotEnoughSpace);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, ResponseFrameUnsupportedDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildResponseFrame(
      sharing::nearby::
          ConnectionResponseFrame_Status_UNSUPPORTED_ATTACHMENT_TYPE);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsResponse(
        answer,
        mojom::ConnectionResponseFrame::Status::kUnsupportedAttachmentType);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, ResponseFrameTimedOutDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildResponseFrame(
      sharing::nearby::ConnectionResponseFrame_Status_TIMED_OUT);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsResponse(
        answer, mojom::ConnectionResponseFrame::Status::kTimedOut);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, ResponseFrameUnknownDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildResponseFrame(
      sharing::nearby::ConnectionResponseFrame_Status_UNKNOWN);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsResponse(
        answer, mojom::ConnectionResponseFrame::Status::kUnknown);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest,
       V1FrameMissingPairedKeyEncryptionFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_PAIRED_KEY_ENCRYPTION);

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, PairedKeyEncryptionFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_PAIRED_KEY_ENCRYPTION);
  sharing::nearby::PairedKeyEncryptionFrame* paired_key =
      v1frame->mutable_paired_key_encryption();
  std::string signed_data = "foo";
  paired_key->set_signed_data(signed_data);
  std::string secret_id_hash = "bar";
  paired_key->set_secret_id_hash(secret_id_hash);

  std::vector<uint8_t> data;
  data.resize(frame.ByteSize());
  ASSERT_TRUE(frame.SerializeToArray(&data[0], frame.ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsPairedKeyEncryption(answer, signed_data, secret_id_hash);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, V1FrameMissingPairedKeyResultFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_PAIRED_KEY_RESULT);

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, CancelFrameSuccessDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_CANCEL);

  std::vector<uint8_t> data;
  data.resize(frame.ByteSize());
  ASSERT_TRUE(frame.SerializeToArray(&data[0], frame.ByteSize()));

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsCancelFrame(answer);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, PairedKeyResultFrameSuccessDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildPairedKeyResultFrame(
      sharing::nearby::PairedKeyResultFrame_Status_SUCCESS);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsPairedKeyResult(
        answer, mojom::PairedKeyResultFrame::Status::kSuccess);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, PairedKeyResultFrameFailDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildPairedKeyResultFrame(
      sharing::nearby::PairedKeyResultFrame_Status_FAIL);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsPairedKeyResult(
        answer, mojom::PairedKeyResultFrame::Status::kFail);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, PairedKeyResultFrameUnableDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildPairedKeyResultFrame(
      sharing::nearby::PairedKeyResultFrame_Status_UNABLE);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsPairedKeyResult(
        answer, mojom::PairedKeyResultFrame::Status::kUnable);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, PairedKeyResultFrameUnknownDecoding) {
  std::unique_ptr<sharing::nearby::Frame> frame = BuildPairedKeyResultFrame(
      sharing::nearby::PairedKeyResultFrame_Status_UNKNOWN);
  std::vector<uint8_t> data;
  data.resize(frame->ByteSize());
  ASSERT_TRUE(frame->SerializeToArray(&data[0], frame->ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsPairedKeyResult(
        answer, mojom::PairedKeyResultFrame::Status::kUnknown);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

TEST_F(NearbySharingDecoderTest, V1FrameMissingCertificateFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_CERTIFICATE_INFO);

  ExpectNullFrame(frame);
}

TEST_F(NearbySharingDecoderTest, CertificateFrameDecoding) {
  sharing::nearby::Frame frame = sharing::nearby::Frame();
  frame.set_version(sharing::nearby::Frame_Version_V1);
  sharing::nearby::V1Frame* v1frame = frame.mutable_v1();
  v1frame->set_type(sharing::nearby::V1Frame_FrameType_CERTIFICATE_INFO);
  sharing::nearby::CertificateInfoFrame* cert_frame =
      v1frame->mutable_certificate_info();

  // Build several PublicCertificate.
  int num_certs = 5;
  std::vector<sharing::nearby::PublicCertificate> certs;
  certs.reserve(num_certs);

  for (int i = 0; i < num_certs; i++) {
    sharing::nearby::PublicCertificate* cert =
        cert_frame->add_public_certificate();
    cert->set_secret_id("secret id " + base::NumberToString(i));
    cert->set_authenticity_key("auth key " + base::NumberToString(i));
    cert->set_public_key("public key " + base::NumberToString(i));
    cert->set_start_time(25);
    cert->set_end_time(30);
    cert->set_encrypted_metadata_bytes("metadata " + base::NumberToString(i));
    cert->set_metadata_encryption_key_tag("tag " + base::NumberToString(i));
    certs.push_back(*cert);
  }

  std::vector<uint8_t> data;
  data.resize(frame.ByteSize());
  ASSERT_TRUE(frame.SerializeToArray(&data[0], frame.ByteSize()));
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting([&](const mojom::FramePtr answer) {
    ExpectFrameContainsCertificateInfo(answer, certs);
    run_loop.Quit();
  });
  decoder()->DecodeFrame(std::move(data), std::move(callback));
  run_loop.Run();
}

}  // namespace sharing
