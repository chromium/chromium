// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/decoder/nearby_decoder.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/services/sharing/nearby/decoder/advertisement_decoder.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"

namespace sharing {

namespace {

mojom::FileMetadata::Type ConvertFileMetadataType(
    sharing::nearby::FileMetadata_Type type) {
  switch (type) {
    case sharing::nearby::FileMetadata_Type_IMAGE:
      return mojom::FileMetadata::Type::kImage;
    case sharing::nearby::FileMetadata_Type_VIDEO:
      return mojom::FileMetadata::Type::kVideo;
    case sharing::nearby::FileMetadata_Type_APP:
      return mojom::FileMetadata::Type::kApp;
    case sharing::nearby::FileMetadata_Type_AUDIO:
      return mojom::FileMetadata::Type::kAudio;
    default:
      return mojom::FileMetadata::Type::kUnknown;
  }
}

std::vector<mojom::FileMetadataPtr> GetFileMetadata(
    const sharing::nearby::IntroductionFrame& proto_frame) {
  std::vector<mojom::FileMetadataPtr> mojo_file_metadatas;
  mojo_file_metadatas.reserve(proto_frame.file_metadata_size());

  for (const sharing::nearby::FileMetadata& metadata :
       proto_frame.file_metadata()) {
    mojo_file_metadatas.push_back(mojom::FileMetadata::New(
        metadata.name(), ConvertFileMetadataType(metadata.type()),
        metadata.payload_id(), metadata.size(), metadata.mime_type(),
        metadata.id()));
  }

  return mojo_file_metadatas;
}

mojom::TextMetadata::Type ConvertTextMetadataType(
    sharing::nearby::TextMetadata_Type type) {
  switch (type) {
    case sharing::nearby::TextMetadata_Type_TEXT:
      return mojom::TextMetadata::Type::kText;
    case sharing::nearby::TextMetadata_Type_URL:
      return mojom::TextMetadata::Type::kUrl;
    case sharing::nearby::TextMetadata_Type_ADDRESS:
      return mojom::TextMetadata::Type::kAddress;
    case sharing::nearby::TextMetadata_Type_PHONE_NUMBER:
      return mojom::TextMetadata::Type::kPhoneNumber;
    default:
      return mojom::TextMetadata::Type::kUnknown;
  }
}

std::vector<mojom::TextMetadataPtr> GetTextMetadata(
    const sharing::nearby::IntroductionFrame& proto_frame) {
  std::vector<mojom::TextMetadataPtr> mojo_text_metadatas;
  mojo_text_metadatas.reserve(proto_frame.text_metadata_size());

  for (const sharing::nearby::TextMetadata& metadata :
       proto_frame.text_metadata()) {
    mojo_text_metadatas.push_back(mojom::TextMetadata::New(
        metadata.text_title(), ConvertTextMetadataType(metadata.type()),
        metadata.payload_id(), metadata.size(), metadata.id()));
  }

  return mojo_text_metadatas;
}

mojom::WifiCredentialsMetadata::SecurityType ConvertSecurityType(
    sharing::nearby::WifiCredentialsMetadata_SecurityType type) {
  switch (type) {
    case sharing::nearby::WifiCredentialsMetadata_SecurityType_OPEN:
      return mojom::WifiCredentialsMetadata::SecurityType::kOpen;
    case sharing::nearby::WifiCredentialsMetadata_SecurityType_WPA_PSK:
      return mojom::WifiCredentialsMetadata::SecurityType::kWpaPsk;
    case sharing::nearby::WifiCredentialsMetadata_SecurityType_WEP:
      return mojom::WifiCredentialsMetadata::SecurityType::kWep;
    default:
      return mojom::WifiCredentialsMetadata::SecurityType::kUnknownSecurityType;
  }
}

std::vector<mojom::WifiCredentialsMetadataPtr> GetWifiMetadata(
    const sharing::nearby::IntroductionFrame& proto_frame) {
  std::vector<mojom::WifiCredentialsMetadataPtr> mojo_wifi_metadatas;
  mojo_wifi_metadatas.reserve(proto_frame.wifi_credentials_metadata_size());

  for (const sharing::nearby::WifiCredentialsMetadata& metadata :
       proto_frame.wifi_credentials_metadata()) {
    mojo_wifi_metadatas.push_back(mojom::WifiCredentialsMetadata::New(
        metadata.ssid(), ConvertSecurityType(metadata.security_type()),
        metadata.payload_id(), metadata.id()));
  }

  return mojo_wifi_metadatas;
}

mojom::IntroductionFramePtr GetIntroductionFrame(
    const sharing::nearby::IntroductionFrame& proto_frame) {
  return mojom::IntroductionFrame::New(
      GetFileMetadata(proto_frame), GetTextMetadata(proto_frame),
      proto_frame.required_package(), GetWifiMetadata(proto_frame));
}

mojom::ConnectionResponseFrame::Status ConvertConnectionResponseStatus(
    sharing::nearby::ConnectionResponseFrame_Status status) {
  switch (status) {
    case sharing::nearby::ConnectionResponseFrame_Status_ACCEPT:
      return mojom::ConnectionResponseFrame::Status::kAccept;
    case sharing::nearby::ConnectionResponseFrame_Status_REJECT:
      return mojom::ConnectionResponseFrame::Status::kReject;
    case sharing::nearby::ConnectionResponseFrame_Status_NOT_ENOUGH_SPACE:
      return mojom::ConnectionResponseFrame::Status::kNotEnoughSpace;
    case sharing::nearby::
        ConnectionResponseFrame_Status_UNSUPPORTED_ATTACHMENT_TYPE:
      return mojom::ConnectionResponseFrame::Status::kUnsupportedAttachmentType;
    case sharing::nearby::ConnectionResponseFrame_Status_TIMED_OUT:
      return mojom::ConnectionResponseFrame::Status::kTimedOut;
    default:
      return mojom::ConnectionResponseFrame::Status::kUnknown;
  }
}

mojom::ConnectionResponseFramePtr GetConnectionResponseFrame(
    const sharing::nearby::ConnectionResponseFrame& proto_frame) {
  return mojom::ConnectionResponseFrame::New(
      ConvertConnectionResponseStatus(proto_frame.status()));
}

mojom::PairedKeyEncryptionFramePtr GetPairedKeyEncryptionFrame(
    const sharing::nearby::PairedKeyEncryptionFrame& proto_frame) {
  std::optional<std::vector<uint8_t>> optional_signed_data =
      proto_frame.has_optional_signed_data()
          ? std::make_optional<std::vector<uint8_t>>(
                proto_frame.optional_signed_data().begin(),
                proto_frame.optional_signed_data().end())
          : std::nullopt;

  return mojom::PairedKeyEncryptionFrame::New(
      std::vector<uint8_t>(proto_frame.signed_data().begin(),
                           proto_frame.signed_data().end()),
      std::vector<uint8_t>(proto_frame.secret_id_hash().begin(),
                           proto_frame.secret_id_hash().end()),
      optional_signed_data);
}

mojom::PairedKeyResultFrame::Status ConvertPairedKeyStatus(
    sharing::nearby::PairedKeyResultFrame_Status status) {
  switch (status) {
    case sharing::nearby::PairedKeyResultFrame_Status_SUCCESS:
      return mojom::PairedKeyResultFrame::Status::kSuccess;
    case sharing::nearby::PairedKeyResultFrame_Status_FAIL:
      return mojom::PairedKeyResultFrame::Status::kFail;
    case sharing::nearby::PairedKeyResultFrame_Status_UNABLE:
      return mojom::PairedKeyResultFrame::Status::kUnable;
    default:
      return mojom::PairedKeyResultFrame::Status::kUnknown;
  }
}

mojom::PairedKeyResultFramePtr GetPairedKeyResultFrame(
    const sharing::nearby::PairedKeyResultFrame& proto_frame) {
  return mojom::PairedKeyResultFrame::New(
      ConvertPairedKeyStatus(proto_frame.status()));
}

mojom::CertificateInfoFramePtr GetCertificateInfoFrame(
    const sharing::nearby::CertificateInfoFrame& proto_frame) {
  std::vector<mojom::PublicCertificatePtr> mojo_certificates;
  mojo_certificates.reserve(proto_frame.public_certificate_size());

  for (const sharing::nearby::PublicCertificate& certificate :
       proto_frame.public_certificate()) {
    std::vector<uint8_t> secret_id(certificate.secret_id().begin(),
                                   certificate.secret_id().end());
    std::vector<uint8_t> authenticity_key(
        certificate.authenticity_key().begin(),
        certificate.authenticity_key().end());
    std::vector<uint8_t> public_key(certificate.public_key().begin(),
                                    certificate.public_key().end());
    std::vector<uint8_t> encrypted_metadata_bytes(
        certificate.encrypted_metadata_bytes().begin(),
        certificate.encrypted_metadata_bytes().end());
    std::vector<uint8_t> metadata_encryption_key_tag(
        certificate.metadata_encryption_key_tag().begin(),
        certificate.metadata_encryption_key_tag().end());
    // Convert timestamp from milliseconds since the Unix epoch.
    base::Time start =
        base::Time::UnixEpoch() + base::Milliseconds(certificate.start_time());
    base::Time end =
        base::Time::UnixEpoch() + base::Milliseconds(certificate.end_time());

    mojo_certificates.push_back(mojom::PublicCertificate::New(
        std::move(secret_id), std::move(authenticity_key),
        std::move(public_key), start, end, std::move(encrypted_metadata_bytes),
        std::move(metadata_encryption_key_tag)));
  }
  return mojom::CertificateInfoFrame::New(std::move(mojo_certificates));
}

}  // namespace

NearbySharingDecoder::NearbySharingDecoder(
    mojo::PendingReceiver<mojom::NearbySharingDecoder> receiver,
    base::OnceClosure on_disconnect)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(on_disconnect));
}

NearbySharingDecoder::~NearbySharingDecoder() = default;

void NearbySharingDecoder::DecodeAdvertisement(
    const std::vector<uint8_t>& data,
    DecodeAdvertisementCallback callback) {
  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::AdvertisementDecoder::FromEndpointInfo(data);

  if (!advertisement) {
    LOG(ERROR) << "Failed to decode advertisement";
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(mojom::Advertisement::New(
      advertisement->salt(), advertisement->encrypted_metadata_key(),
      advertisement->device_type(), advertisement->device_name()));
}

void NearbySharingDecoder::DecodeFrame(const std::vector<uint8_t>& data,
                                       DecodeFrameCallback callback) {
  // Parse incoming data into a protobuf.
  sharing::nearby::Frame proto_frame;

  if (!proto_frame.ParseFromArray(data.data(), data.size())) {
    LOG(ERROR) << "Failed to parse incoming frame";
    std::move(callback).Run(nullptr);
    return;
  }

  if (!proto_frame.has_version() ||
      proto_frame.version() != sharing::nearby::Frame_Version_V1) {
    LOG(ERROR) << "Invalid or missing incoming frame version";
    std::move(callback).Run(nullptr);
    return;
  }

  if (!proto_frame.has_v1()) {
    LOG(ERROR) << "Missing incoming v1frame";
    std::move(callback).Run(nullptr);
    return;
  }

  // Determine the frame type of the protobuf.
  sharing::nearby::V1Frame::FrameType proto_frame_type =
      proto_frame.v1().type();
  mojom::V1FramePtr mojo_v1frame;

  switch (proto_frame_type) {
    case sharing::nearby::V1Frame_FrameType_INTRODUCTION:
      if (!proto_frame.v1().has_introduction()) {
        LOG(ERROR) << "No IntroductionFrame when one expected.";
        std::move(callback).Run(nullptr);
        return;
      }

      mojo_v1frame = mojom::V1Frame::NewIntroduction(
          GetIntroductionFrame(proto_frame.v1().introduction()));
      break;
    case sharing::nearby::V1Frame_FrameType_RESPONSE:
      if (!proto_frame.v1().has_connection_response()) {
        LOG(ERROR) << "No ConnectionResponse when one expected.";
        std::move(callback).Run(nullptr);
        return;
      }

      mojo_v1frame = mojom::V1Frame::NewConnectionResponse(
          GetConnectionResponseFrame(proto_frame.v1().connection_response()));
      break;
    case sharing::nearby::V1Frame_FrameType_PAIRED_KEY_ENCRYPTION:
      if (!proto_frame.v1().has_paired_key_encryption()) {
        LOG(ERROR) << "No PairedKeyEncryption when one expected.";
        std::move(callback).Run(nullptr);
        return;
      }

      mojo_v1frame =
          mojom::V1Frame::NewPairedKeyEncryption(GetPairedKeyEncryptionFrame(
              proto_frame.v1().paired_key_encryption()));
      break;
    case sharing::nearby::V1Frame_FrameType_PAIRED_KEY_RESULT:
      if (!proto_frame.v1().has_paired_key_result()) {
        LOG(ERROR) << "No PairedKeyResult when one expected.";
        std::move(callback).Run(nullptr);
        return;
      }

      mojo_v1frame = mojom::V1Frame::NewPairedKeyResult(
          GetPairedKeyResultFrame(proto_frame.v1().paired_key_result()));
      break;
    case sharing::nearby::V1Frame_FrameType_CERTIFICATE_INFO:
      if (!proto_frame.v1().has_certificate_info()) {
        LOG(ERROR) << "No CertificateInfo when one expected.";
        std::move(callback).Run(nullptr);
        return;
      }

      mojo_v1frame = mojom::V1Frame::NewCertificateInfo(
          GetCertificateInfoFrame(proto_frame.v1().certificate_info()));
      break;
    case sharing::nearby::V1Frame_FrameType_CANCEL:
      mojo_v1frame = mojom::V1Frame::NewCancelFrame(mojom::CancelFrame::New());
      break;
    default:
      LOG(ERROR) << "Unknown type of v1frame, unable to process.";
      std::move(callback).Run(nullptr);
      return;
  }

  std::move(callback).Run(mojom::Frame::NewV1(std::move(mojo_v1frame)));
}

}  // namespace sharing
