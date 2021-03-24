/**
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Constructs an RTCStats dictionary by merging the parent RTCStats object with
 * the dictionary of RTCStats members defined for this dictionary.
 */
function RTCStats(parent, membersObject) {
  if (parent != null)
    Object.assign(this, parent);
  Object.assign(this, membersObject);
}

const Presence = {
  MANDATORY: true,
  OPTIONAL: false,
};

/**
 * According to spec, multiple stats dictionaries can have the same
 * "RTCStats.type". For example, "track" refers to any of
 * RTCSenderVideoTrackAttachmentStats, RTCSenderAudioTrackAttachmentStats,
 * RTCReceiverVideoTrackAttachmentStats and
 * RTCReceiverAudioTrackAttachmentStats. Inspection is needed to determine which
 * dictionary applies to the object; for simplicity, this class merges all of
 * the associated dictionaries into a single dictionary.
 */
class MergedRTCStats {
  constructor(presence, type, stats) {
    this.presence_ = presence;
    this.type_ = type;
    this.stats_ = stats;
  }

  presence() {
    return this.presence_;
  }

  type() {
    return this.type_;
  }

  stats() {
    return this.stats_;
  }

  merge(presence, stats) {
    if (presence)
      this.presence_ = true;
    Object.assign(this.stats_, stats);
  }
}

/**
 * Maps "RTCStats.type" values to MergedRTCStats. These are descriptions of
 * whitelisted (allowed to be exposed to the web) RTCStats-derived dictionaries,
 * see RTCStats definitions below.
 * @private
 */
const gStatsWhitelist = new Map();

function addRTCStatsToWhitelist(presence, type, stats) {
  mergedStats = gStatsWhitelist.get(type);
  if (!mergedStats) {
    gStatsWhitelist.set(type, new MergedRTCStats(presence, type, stats));
  } else {
    mergedStats.merge(presence, stats);
  }
}

/**
 * RTCRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#streamstats-dict*
 * @private
 */
let kRTCRtpStreamStats = new RTCStats(null, {
  ssrc: 'number',
  isRemote: 'boolean',  // Obsolete, type reveals if "remote-" or not.
  kind: 'string',
  mediaType: 'string',  // Obsolete, replaced by |kind|.
  transportId: 'string',
  codecId: 'string',
});

/**
 * RTCReceivedRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats
 * @private
 */
let kRTCReceivedRtpStreamStats = new RTCStats(kRTCRtpStreamStats, {
  packetsReceived: 'number',
  packetsLost: 'number',
  jitter: 'number',
  packetsDiscarded: 'number',
  packetsRepaired: 'number',
  burstPacketsLost: 'number',
  burstPacketsDiscarded: 'number',
  burstLossCount: 'number',
  burstDiscardCount: 'number',
  burstLossRate: 'number',
  burstDiscardRate: 'number',
  gapLossRate: 'number',
  gapDiscardRate: 'number',
});

/*
 * RTCInboundRTPStreamStats
 * https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
 * @private
 */
let kRTCInboundRtpStreamStats = new RTCStats(kRTCReceivedRtpStreamStats, {
  trackId: 'string',
  receiverId: 'string',
  remoteId: 'string',
  framesDecoded: 'number',
  keyFramesDecoded: 'number',
  frameBitDepth: 'number',
  qpSum: 'number',
  totalDecodeTime: 'number',
  totalInterFrameDelay: 'number',
  totalSquaredInterFrameDelay: 'number',
  lastPacketReceivedTimestamp: 'number',
  averageRtcpInterval: 'number',
  fecPacketsReceived: 'number',
  fecPacketsDiscarded: 'number',
  bytesReceived: 'number',
  headerBytesReceived: 'number',
  packetsFailedDecryption: 'number',
  packetsDuplicated: 'number',
  perDscpPacketsReceived: 'object',
  nackCount: 'number',
  firCount: 'number',
  pliCount: 'number',
  sliCount: 'number',
  frameWidth: 'number',
  frameHeight: 'number',
  frameBitDepth: 'number',
  framesPerSecond: 'number',
  jitterBufferDelay: 'number',
  jitterBufferEmittedCount: 'number',
  totalSamplesReceived: 'number',
  concealedSamples: 'number',
  silentConcealedSamples: 'number',
  concealmentEvents: 'number',
  insertedSamplesForDeceleration: 'number',
  removedSamplesForAcceleration: 'number',
  audioLevel: 'number',
  totalAudioEnergy: 'number',
  totalSamplesDuration: 'number',
  framesReceived: 'number',
  framesDropped: 'number',
  estimatedPlayoutTimestamp: 'number',
  fractionLost: 'number',  // Obsolete, moved to RTCRemoteInboundRtpStreamStats.
  decoderImplementation: 'string',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'inbound-rtp', kRTCInboundRtpStreamStats);

/*
 * RTCRemoteInboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#remoteinboundrtpstats-dict*
 * @private
 */
let kRTCRemoteInboundRtpStreamStats =
    new RTCStats(kRTCReceivedRtpStreamStats, {
  localId: 'string',
  roundTripTime: 'number',
  fractionLost: 'number',
  totalRoundTripTime: 'number',
  roundTripTimeMeasurements: 'number',
});
// TODO(https://crbug.com/967382): Update the browser_tests to wait for the
// existence of remote-inbound-rtp as well (these are created later than
// outbound-rtp). When this is done, change presence to MANDATORY.
addRTCStatsToWhitelist(
    Presence.OPTIONAL, 'remote-inbound-rtp', kRTCRemoteInboundRtpStreamStats);

/**
 * RTCSentRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcsentrtpstreamstats
 * @private
 */
let kRTCSentRtpStreamStats = new RTCStats(kRTCRtpStreamStats, {
  packetsSent: 'number',
  packetsDiscardedOnSend: 'number',
  fecPacketsSent: 'number',
  bytesSent: 'number',
  bytesDiscardedOnSend: 'number',
});

/*
 * RTCOutboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#outboundrtpstats-dict*
 * @private
 */
let kRTCOutboundRtpStreamStats = new RTCStats(kRTCSentRtpStreamStats, {
  trackId: 'string',
  mediaSourceId: 'string',
  senderId: 'string',
  remoteId: 'string',
  lastPacketSentTimestamp: 'number',
  retransmittedPacketsSent: 'number',
  retransmittedBytesSent: 'number',
  headerBytesSent: 'number',
  targetBitrate: 'number',
  totalEncodedBytesTarget: 'number',
  frameBitDepth: 'number',
  framesEncoded: 'number',
  keyFramesEncoded: 'number',
  qpSum: 'number',
  totalEncodeTime: 'number',
  totalPacketSendDelay: 'number',
  averageRtcpInterval: 'number',
  qualityLimitationReason: 'string',
  qualityLimitationDurations: 'object',
  qualityLimitationResolutionChanges: 'number',
  perDscpPacketsSent: 'object',
  nackCount: 'number',
  firCount: 'number',
  pliCount: 'number',
  sliCount: 'number',
  encoderImplementation: 'string',
  rid: 'string',
  frameWidth: 'number',
  frameHeight: 'number',
  framesPerSecond: 'number',
  framesSent: 'number',
  hugeFramesSent: 'number',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'outbound-rtp', kRTCOutboundRtpStreamStats);

/*
 * RTCRemoteOutboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcremoteoutboundrtpstreamstats
 * @private
 */
let kRTCRemoteOutboundRtpStreamStats = new RTCStats(kRTCSentRtpStreamStats, {
  localId: 'string',
  remoteTimestamp: 'number',
  reportsSent: 'number',
});
// TODO(hbos): When remote-outbound-rtp is implemented, make presence MANDATORY.
addRTCStatsToWhitelist(
    Presence.OPTIONAL, 'remote-outbound-rtp', kRTCRemoteOutboundRtpStreamStats);

/**
 * RTCMediaSourceStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcmediasourcestats
 * @private
 */
const kRTCMediaSourceStats = new RTCStats(null, {
  trackIdentifier: 'string',
  kind: 'string',
});

/**
 * RTCAudioSourceStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcaudiosourcestats
 * @private
 */
const kRTCAudioSourceStats = new RTCStats(kRTCMediaSourceStats, {
  audioLevel: 'number',
  totalAudioEnergy: 'number',
  totalSamplesDuration: 'number',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'media-source', kRTCAudioSourceStats);

/**
 * RTCVideoSourceStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcvideosourcestats
 * @private
 */
const kRTCVideoSourceStats = new RTCStats(kRTCMediaSourceStats, {
  width: 'number',
  height: 'number',
  bitDepth: 'number',
  frames: 'number',
  framesPerSecond: 'number',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'media-source', kRTCVideoSourceStats);

/*
 * RTCRtpContributingSourceStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcrtpcontributingsourcestats
 * @private
 */
let kRTCRtpContributingSourceStats = new RTCStats(null, {
  contributorSsrc: 'number',
  inboundRtpStreamId: 'string',
  packetsContributedTo: 'number',
  audioLevel: 'number',
});
// TODO(hbos): When csrc is implemented, make presence MANDATORY.
addRTCStatsToWhitelist(
    Presence.OPTIONAL, 'csrc', kRTCRtpContributingSourceStats);

/*
 * RTCCodecStats
 * https://w3c.github.io/webrtc-stats/#codec-dict*
 * @private
 */
let kRTCCodecStats = new RTCStats(null, {
  transportId: 'string',
  payloadType: 'number',
  mimeType: 'string',
  clockRate: 'number',
  channels: 'number',
  sdpFmtpLine: 'string',
});
addRTCStatsToWhitelist(Presence.MANDATORY, 'codec', kRTCCodecStats);

/*
 * RTCPeerConnectionStats
 * https://w3c.github.io/webrtc-stats/#pcstats-dict*
 * @private
 */
let kRTCPeerConnectionStats = new RTCStats(null, {
  dataChannelsOpened: 'number',
  dataChannelsClosed: 'number',
  dataChannelsRequested: 'number',
  dataChannelsAccepted: 'number',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'peer-connection', kRTCPeerConnectionStats);

/*
 * RTCMediaStreamStats
 * https://w3c.github.io/webrtc-stats/#msstats-dict*
 * @private
 */
let kRTCMediaStreamStats = new RTCStats(null, {
  streamIdentifier: 'string',
  trackIds: 'sequence_string',
});
addRTCStatsToWhitelist(Presence.MANDATORY, 'stream', kRTCMediaStreamStats);

/**
 * RTCMediaHandlerStats
 * https://w3c.github.io/webrtc-stats/#mststats-dict*
 * @private
 */
let kRTCMediaHandlerStats = new RTCStats(null, {
  trackIdentifier: 'string',
  remoteSource: 'boolean',
  ended: 'boolean',
  kind: 'string',
  priority: 'string',
  detached: 'boolean',  // Obsolete, detached stats should fire "onstatsended".
});

/*
 * RTCVideoHandlerStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcvideohandlerstats
 * @private
 */
let kRTCVideoHandlerStats = new RTCStats(kRTCMediaHandlerStats, {
  frameWidth: 'number',
  frameHeight: 'number',
  framesPerSecond: 'number',
});

/*
 * RTCVideoSenderStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcvideosenderstats
 * @private
 */
let kRTCVideoSenderStats = new RTCStats(kRTCVideoHandlerStats, {
  mediaSourceId: 'string',
  framesCaptured: 'number',
  framesSent: 'number',
  hugeFramesSent: 'number',
});
// TODO(hbos): When sender is implemented, make presence MANDATORY.
addRTCStatsToWhitelist(Presence.OPTIONAL, 'sender', kRTCVideoSenderStats);

/*
 * RTCSenderVideoTrackAttachmentStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcsendervideotrackattachmentstats
 * @private
 */
let kRTCSenderVideoTrackAttachmentStats = new RTCStats(kRTCVideoSenderStats, {
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'track', kRTCSenderVideoTrackAttachmentStats);

/*
 * RTCVideoReceiverStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcvideoreceiverstats
 * @private
 */
let kRTCVideoReceiverStats = new RTCStats(kRTCVideoHandlerStats, {
  estimatedPlayoutTimestamp: 'number',
  jitterBufferDelay: 'number',
  jitterBufferEmittedCount: 'number',
  framesReceived: 'number',
  framesDecoded: 'number',
  framesDropped: 'number',
  partialFramesLost: 'number',
  fullFramesLost: 'number',
});
// TODO(hbos): When receiver is implemented, make presence MANDATORY.
addRTCStatsToWhitelist(
    Presence.OPTIONAL, 'receiver', kRTCVideoReceiverStats);

/*
 * RTCReceiverVideoTrackAttachmentStats
 * https://github.com/w3c/webrtc-stats/issues/424
 * @private
 */
let kRTCReceiverVideoTrackAttachmentStats =
    new RTCStats(kRTCVideoReceiverStats, {
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'track', kRTCReceiverVideoTrackAttachmentStats);

/*
 * RTCAudioHandlerStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcaudiohandlerstats
 * @private
 */
let kRTCAudioHandlerStats = new RTCStats(kRTCMediaHandlerStats, {
  audioLevel: 'number',
  totalAudioEnergy: 'number',
  voiceActivityFlag: 'boolean',
  totalSamplesDuration: 'number',
});

/*
 * RTCAudioSenderStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcaudiosenderstats
 * @private
 */
let kRTCAudioSenderStats = new RTCStats(kRTCAudioHandlerStats, {
  mediaSourceId: 'string',
  echoReturnLoss: 'number',
  echoReturnLossEnhancement: 'number',
  totalSamplesSent: 'number',
});
// TODO(hbos): When sender is implemented, make presence MANDATORY.
addRTCStatsToWhitelist(Presence.OPTIONAL, 'sender', kRTCAudioSenderStats);

/*
 * RTCSenderAudioTrackAttachmentStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcsenderaudiotrackattachmentstats
 * @private
 */
let kRTCSenderAudioTrackAttachmentStats = new RTCStats(kRTCAudioSenderStats, {
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'track', kRTCSenderAudioTrackAttachmentStats);

/*
 * RTCAudioReceiverStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcaudioreceiverstats
 * @private
 */
let kRTCAudioReceiverStats = new RTCStats(kRTCAudioHandlerStats, {
  estimatedPlayoutTimestamp: 'number',
  jitterBufferDelay: 'number',
  jitterBufferEmittedCount: 'number',
  totalSamplesReceived: 'number',
  concealedSamples: 'number',
  silentConcealedSamples: 'number',
  concealmentEvents: 'number',
  insertedSamplesForDeceleration: 'number',
  removedSamplesForAcceleration: 'number',
});
// TODO(hbos): When receiver is implemented, make presence MANDATORY.
addRTCStatsToWhitelist(
    Presence.OPTIONAL, 'receiver', kRTCAudioReceiverStats);

/*
 * RTCReceiverAudioTrackAttachmentStats
 * https://github.com/w3c/webrtc-stats/issues/424
 * @private
 */
let kRTCReceiverAudioTrackAttachmentStats =
    new RTCStats(kRTCAudioReceiverStats, {
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'track', kRTCReceiverAudioTrackAttachmentStats);

/*
 * RTCDataChannelStats
 * https://w3c.github.io/webrtc-stats/#dcstats-dict*
 * @private
 */
let kRTCDataChannelStats = new RTCStats(null, {
  label: 'string',
  protocol: 'string',
  dataChannelIdentifier: 'number',
  state: 'string',
  messagesSent: 'number',
  bytesSent: 'number',
  messagesReceived: 'number',
  bytesReceived: 'number',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'data-channel', kRTCDataChannelStats);

/*
 * RTCTransportStats
 * https://w3c.github.io/webrtc-stats/#transportstats-dict*
 * @private
 */
let kRTCTransportStats = new RTCStats(null, {
  bytesSent: 'number',
  packetsSent: 'number',
  bytesReceived: 'number',
  packetsReceived: 'number',
  rtcpTransportStatsId: 'string',
  dtlsState: 'string',
  selectedCandidatePairId: 'string',
  localCertificateId: 'string',
  remoteCertificateId: 'string',
  tlsVersion: 'string',
  dtlsCipher: 'string',
  srtpCipher: 'string',
  selectedCandidatePairChanges: 'number',
});
addRTCStatsToWhitelist(Presence.MANDATORY, 'transport', kRTCTransportStats);

/*
 * RTCIceCandidateStats
 * https://w3c.github.io/webrtc-stats/#icecandidate-dict*
 * @private
 */
let kRTCIceCandidateStats = new RTCStats(null, {
  transportId: 'string',
  isRemote: 'boolean',
  networkType: 'string',
  ip: 'string',
  address: 'string',
  port: 'number',
  protocol: 'string',
  relayProtocol: 'string',
  candidateType: 'string',
  priority: 'number',
  url: 'string',
  deleted: 'boolean',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'local-candidate', kRTCIceCandidateStats);
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'remote-candidate', kRTCIceCandidateStats);

/*
 * RTCIceCandidatePairStats
 * https://w3c.github.io/webrtc-stats/#candidatepair-dict*
 * @private
 */
let kRTCIceCandidatePairStats = new RTCStats(null, {
  transportId: 'string',
  localCandidateId: 'string',
  remoteCandidateId: 'string',
  state: 'string',
  priority: 'number',
  nominated: 'boolean',
  writable: 'boolean',
  readable: 'boolean',
  bytesSent: 'number',
  bytesReceived: 'number',
  totalRoundTripTime: 'number',
  currentRoundTripTime: 'number',
  availableOutgoingBitrate: 'number',
  availableIncomingBitrate: 'number',
  requestsReceived: 'number',
  requestsSent: 'number',
  responsesReceived: 'number',
  responsesSent: 'number',
  retransmissionsReceived: 'number',
  retransmissionsSent: 'number',
  consentRequestsReceived: 'number',
  consentRequestsSent: 'number',
  consentResponsesReceived: 'number',
  consentResponsesSent: 'number',
});
addRTCStatsToWhitelist(
    Presence.MANDATORY, 'candidate-pair', kRTCIceCandidatePairStats);

/*
 * RTCCertificateStats
 * https://w3c.github.io/webrtc-stats/#certificatestats-dict*
 * @private
 */
let kRTCCertificateStats = new RTCStats(null, {
  fingerprint: 'string',
  fingerprintAlgorithm: 'string',
  base64Certificate: 'string',
  issuerCertificateId: 'string',
});
addRTCStatsToWhitelist(Presence.MANDATORY, 'certificate', kRTCCertificateStats);

// Public interface to tests. These are expected to be called with
// ExecuteJavascript invocations from the browser tests and will return answers
// through the DOM automation controller.

/**
 * Verifies that the promise-based |RTCPeerConnection.getStats| returns stats,
 * makes sure that all returned stats have the base RTCStats-members and that
 * all stats are allowed by the whitelist.
 *
 * Returns "ok-" followed by JSON-stringified array of "RTCStats.type" values
 * to the test, these being the different types of stats that was returned by
 * this call to getStats.
 */
function verifyStatsGeneratedPromise() {
  peerConnection_().getStats()
    .then(function(report) {
      if (report == null || report.size == 0)
        throw new failTest('report is null or empty.');
      let statsTypes = new Set();
      let ids = new Set();
      for (let stats of report.values()) {
        verifyStatsIsWhitelisted_(stats);
        statsTypes.add(stats.type);
        if (ids.has(stats.id))
          throw failTest('stats.id is not a unique identifier.');
        ids.add(stats.id);
      }
      returnToTest('ok-' + JSON.stringify(Array.from(statsTypes.values())));
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}

/**
 * Gets the result of the promise-based |RTCPeerConnection.getStats| as a
 * dictionary of RTCStats-dictionaries.
 *
 * Returns "ok-" followed by a JSON-stringified dictionary of dictionaries to
 * the test.
 */
function getStatsReportDictionary() {
  peerConnection_().getStats()
    .then(function(report) {
      if (report == null || report.size == 0)
        throw new failTest('report is null or empty.');
      let reportDictionary = {};
      for (let stats of report.values()) {
        reportDictionary[stats.id] = stats;
      }
      returnToTest('ok-' + JSON.stringify(reportDictionary));
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}

/**
 * Measures the performance of the promise-based |RTCPeerConnection.getStats|
 * and returns the time it took in milliseconds as a double
 * (DOMHighResTimeStamp, accurate to one thousandth of a millisecond).
 * Verifies that all stats types of the whitelist were contained in the result.
 *
 * Returns "ok-" followed by a double on success.
 */
function measureGetStatsPerformance() {
  let t0 = performance.now();
  peerConnection_().getStats()
    .then(function(report) {
      let t1 = performance.now();
      for (let stats of report.values()) {
        verifyStatsIsWhitelisted_(stats);
      }
      for (let mandatoryType of mandatoryStatsTypes()) {
        let mandatoryTypeExists = false;
        for (let stats of report.values()) {
          if (stats.type == mandatoryType) {
            mandatoryTypeExists = true;
            break;
          }
        }
        if (!mandatoryTypeExists) {
          returnToTest('Missing mandatory type: ' + mandatoryType);
        }
      }
      returnToTest('ok-' + (t1 - t0));
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}

/**
 * Returns a complete list of mandatory "RTCStats.type" values from the
 * whitelist as a JSON-stringified array of strings to the test.
 */
function getMandatoryStatsTypes() {
  returnToTest(JSON.stringify(Array.from(mandatoryStatsTypes())));
}

// Internals.

/** Gets a set of all mandatory stats types. */
function mandatoryStatsTypes() {
  const mandatoryTypes = new Set();
  for (let whitelistedStats of gStatsWhitelist.values()) {
    if (whitelistedStats.presence() == Presence.MANDATORY)
      mandatoryTypes.add(whitelistedStats.type());
  }
  return mandatoryTypes;
}

/**
 * Checks if |stats| correctly maps to a a whitelisted RTCStats-derived
 * dictionary, throwing |failTest| if it doesn't. See |gStatsWhitelist|.
 *
 * The "RTCStats.type" must map to a known dictionary description. Every member
 * is optional, but if present it must be present in the whitelisted dictionary
 * description and its type must match.
 * @private
 */
function verifyStatsIsWhitelisted_(stats) {
  if (stats == null)
    throw failTest('stats is null or undefined: ' + stats);
  if (typeof(stats.id) !== 'string')
    throw failTest('stats.id is not a string:' + stats.id);
  if (typeof(stats.timestamp) !== 'number' || !isFinite(stats.timestamp) ||
      stats.timestamp <= 0) {
    throw failTest('stats.timestamp is not a positive finite number: ' +
        stats.timestamp);
  }
  if (typeof(stats.type) !== 'string')
    throw failTest('stats.type is not a string: ' + stats.type);
  let whitelistedStats = gStatsWhitelist.get(stats.type);
  if (whitelistedStats == null)
    throw failTest('stats.type is not a whitelisted type: ' + stats.type);
  whitelistedStats = whitelistedStats.stats();
  for (let propertyName in stats) {
    if (propertyName === 'id' || propertyName === 'timestamp' ||
        propertyName === 'type') {
      continue;
    }
    if (!whitelistedStats.hasOwnProperty(propertyName)) {
      throw failTest(
          stats.type + '.' + propertyName + ' is not a whitelisted ' +
          'member: ' + stats[propertyName]);
    }
    if (whitelistedStats[propertyName] === 'any')
      continue;
    if (!whitelistedStats[propertyName].startsWith('sequence_')) {
      if (typeof(stats[propertyName]) !== whitelistedStats[propertyName]) {
        throw failTest('stats.' + propertyName + ' should have a different ' +
            'type according to the whitelist: ' + stats[propertyName] + ' vs ' +
            whitelistedStats[propertyName]);
      }
    } else {
      if (!Array.isArray(stats[propertyName])) {
        throw failTest('stats.' + propertyName + ' should have a different ' +
            'type according to the whitelist (should be an array): ' +
            JSON.stringify(stats[propertyName]) + ' vs ' +
            whitelistedStats[propertyName]);
      }
      let elementType = whitelistedStats[propertyName].substring(9);
      for (let element in stats[propertyName]) {
        if (typeof(element) !== elementType) {
          throw failTest('stats.' + propertyName + ' should have a different ' +
              'type according to the whitelist (an element of the array has ' +
              'the incorrect type): ' + JSON.stringify(stats[propertyName]) +
              ' vs ' + whitelistedStats[propertyName]);
        }
      }
    }
  }
}
