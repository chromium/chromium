// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_OUTPUT_RESTRICTIONS_H_
#define CHROMECAST_PUBLIC_OUTPUT_RESTRICTIONS_H_

namespace chromecast {

// The below values were adapted from the msdn article on Output Protection
// Levels: https://msdn.microsoft.com/en-us/library/dn468832.aspx

// Restrictions supported/applied to Uncompressed Digital Audio outputs, e.g.
// HDMI, DisplayPort, MHL.
class UncompressedDigitalAudio {
 public:
  enum Restrictions {
    // HDCP protection.
    kHdcp = 1,
    // HDCP protection *or* SCMS Copy Never protection.
    kHdcpOrScmsCopyNever = 1 << 1,
    // Disable the output so that content is not sent to it.
    kDisableOutput = 1 << 2,
  };
};

// Restrictions supported/applied to Compressed Digital Audio outputs, e.g.
// HDMI, DisplayPort, MHL.
class CompressedDigitalAudio {
 public:
  enum Restrictions {
    // HDCP protection *or* SCMS Copy Never protection.
    kHdcpOrScmsCopyNever = 1,
    // HDCP protection.
    kHdcp = 1 << 1,
    // Disable the output so that content is not sent to it.
    kDisableOutput = 1 << 2,
  };
};

// Restrictions supported/applied to Analog TV outputs, e.g. Component,
// Composite, VGA.
class AnalogVideo {
 public:
  enum Restrictions {
    // Content is constrainted to 520,000 pixels of effective resolution.
    kConstrainOutputToSd = 1,
    // CGMS-A Copy Never protection.
    kCgmsACopyNever = 1 << 1,
    // Macrovision APC Automatic Gain Control and Color Stripe protection.
    kAgcAndColorStripe = 1 << 2,
    // Disable the output so that content is not sent to it.
    kDisableOutput = 1 << 3,
  };
};

// Restrictions supported/applied to Uncompressed Digital Video outputs, e.g.
// HDMI, DVI, DisplayPort, MHL.
class UncompressedDigitalVideo {
 public:
  enum Restrictions {
    // HDCP protection.
    kHdcp = 1,
    // Disable the output so that content is not sent to it.
    kDisableOutput = 1 << 2,
  };
};

// Represents either a set of supported OutputRestrictions, or a set of
// OutputRestrictions to be applied.
struct OutputRestrictions {
  OutputRestrictions()
      : uncompressed_digital_audio_mask(0),
        compressed_digital_audio_mask(0),
        analog_video_mask(0),
        uncompressed_digital_video_mask(0) {}

  // A bitmask of UncompressedDigitalAudio restrictions, indicating either
  // supported or required restrictions.
  int uncompressed_digital_audio_mask;

  // A bitmask of CompressedDigitalAudio restrictions, indicating either
  // supported or required restrictions.
  int compressed_digital_audio_mask;

  // A bitmask of Analog TV restrictions, indicating either supported or
  // required restrictions.
  int analog_video_mask;

  // A bitmask of UncompressedDigitalVideo restrictions, indicating either
  // supported or required restrictions.
  int uncompressed_digital_video_mask;
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_OUTPUT_RESTRICTIONS_H_
