/**
 * Copyright 2016 The Chromium Authors
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
 * allowlisted (allowed to be exposed to the web) RTCStats-derived dictionaries,
 * see RTCStats definitions below.
 * @private
 */
const gStatsAllowlist = new Map();

function addRTCStatsToAllowlist(presence, type, stats) {
  mergedStats = gStatsAllowlist.get(type);
  if (!mergedStats) {
    gStatsAllowlist.set(type, new MergedRTCStats(presence, type, stats));
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
  });

/*
 * RTCInboundRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
 * @private
 */
let kRTCInboundRtpStreamStats = new RTCStats(kRTCReceivedRtpStreamStats, {
  trackId: 'string',
  trackIdentifier: 'string',
  mid: 'string',
  remoteId: 'string',
  framesDecoded: 'number',
  keyFramesDecoded: 'number',
  qpSum: 'number',
  totalDecodeTime: 'number',
  totalProcessingDelay: 'number',
  totalInterFrameDelay: 'number',
  totalSquaredInterFrameDelay: 'number',
  freezeCount: 'number',
  pauseCount: 'number',
  totalFreezesDuration: 'number',
  totalPausesDuration: 'number',
  lastPacketReceivedTimestamp: 'number',
  fecPacketsReceived: 'number',
  fecPacketsDiscarded: 'number',
  bytesReceived: 'number',
  headerBytesReceived: 'number',
  packetsDiscarded: 'number',
  nackCount: 'number',
  firCount: 'number',
  pliCount: 'number',
  frameWidth: 'number',
  frameHeight: 'number',
  framesPerSecond: 'number',
  jitterBufferDelay: 'number',
  jitterBufferTargetDelay: 'number',
  jitterBufferMinimumDelay: 'number',
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
  playoutId: 'string',
  powerEfficientDecoder: 'boolean',
  framesAssembledFromMultiplePackets: 'number',
  totalAssemblyTime: 'number',
  googTimingFrameInfo: 'string',
});
addRTCStatsToAllowlist(
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
addRTCStatsToAllowlist(
    Presence.OPTIONAL, 'remote-inbound-rtp', kRTCRemoteInboundRtpStreamStats);

/**
 * RTCSentRtpStreamStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcsentrtpstreamstats
 * @private
 */
let kRTCSentRtpStreamStats = new RTCStats(kRTCRtpStreamStats, {
  packetsSent: 'number',
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
  remoteId: 'string',
  mid: 'string',
  retransmittedPacketsSent: 'number',
  retransmittedBytesSent: 'number',
  headerBytesSent: 'number',
  targetBitrate: 'number',
  totalEncodedBytesTarget: 'number',
  framesEncoded: 'number',
  keyFramesEncoded: 'number',
  qpSum: 'number',
  totalEncodeTime: 'number',
  totalPacketSendDelay: 'number',
  qualityLimitationReason: 'string',
  qualityLimitationDurations: 'object',
  qualityLimitationResolutionChanges: 'number',
  nackCount: 'number',
  firCount: 'number',
  pliCount: 'number',
  encoderImplementation: 'string',
  rid: 'string',
  frameWidth: 'number',
  frameHeight: 'number',
  framesPerSecond: 'number',
  framesSent: 'number',
  hugeFramesSent: 'number',
  active: 'boolean',
  powerEfficientEncoder: 'boolean',
  scalabilityMode: 'string',
});
addRTCStatsToAllowlist(
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
  roundTripTime: 'number',
  totalRoundTripTime: 'number',
  roundTripTimeMeasurements: 'number',
});
// TODO(hbos): When remote-outbound-rtp is implemented, make presence MANDATORY.
addRTCStatsToAllowlist(
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
  echoReturnLoss: 'number',
  echoReturnLossEnhancement: 'number',
});
addRTCStatsToAllowlist(
    Presence.MANDATORY, 'media-source', kRTCAudioSourceStats);

/**
 * RTCVideoSourceStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcvideosourcestats
 * @private
 */
const kRTCVideoSourceStats = new RTCStats(kRTCMediaSourceStats, {
  width: 'number',
  height: 'number',
  frames: 'number',
  framesPerSecond: 'number',
});
addRTCStatsToAllowlist(
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
addRTCStatsToAllowlist(
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
addRTCStatsToAllowlist(Presence.MANDATORY, 'codec', kRTCCodecStats);

/*
 * RTCPeerConnectionStats
 * https://w3c.github.io/webrtc-stats/#pcstats-dict*
 * @private
 */
let kRTCPeerConnectionStats = new RTCStats(null, {
  dataChannelsOpened: 'number',
  dataChannelsClosed: 'number',
});
addRTCStatsToAllowlist(
    Presence.MANDATORY, 'peer-connection', kRTCPeerConnectionStats);

/*
 * RTCMediaStreamStats
 * https://w3c.github.io/webrtc-stats/#obsolete-rtcmediastreamstats-members
 * @private
 */
let kRTCMediaStreamStats = new RTCStats(null, {
  streamIdentifier: 'string',
  trackIds: 'sequence_string',
});
// It's OPTIONAL because this dictionary has become obsolete.
addRTCStatsToAllowlist(Presence.OPTIONAL, 'stream', kRTCMediaStreamStats);

/**
 * RTCMediaStreamTrackStats
 * https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats
 * @private
 */
let kRTCMediaStreamTrackStats = new RTCStats('track', {
  trackIdentifier: 'string',
  remoteSource: 'boolean',
  ended: 'boolean',
  kind: 'string',
  priority: 'string',
  detached: 'boolean',  // Obsolete, detached stats should fire "onstatsended".
  frameWidth: 'number',
  frameHeight: 'number',
  mediaSourceId: 'string',
  framesCaptured: 'number',
  framesSent: 'number',
  hugeFramesSent: 'number',
  estimatedPlayoutTimestamp: 'number',
  jitterBufferDelay: 'number',
  jitterBufferEmittedCount: 'number',
  framesReceived: 'number',
  framesDecoded: 'number',
  framesDropped: 'number',
  audioLevel: 'number',
  totalAudioEnergy: 'number',
  totalSamplesDuration: 'number',
  echoReturnLoss: 'number',
  echoReturnLossEnhancement: 'number',
  totalSamplesReceived: 'number',
  concealedSamples: 'number',
  silentConcealedSamples: 'number',
  concealmentEvents: 'number',
  insertedSamplesForDeceleration: 'number',
  removedSamplesForAcceleration: 'number',
});
// It's OPTIONAL because this dictionary has become obsolete.
addRTCStatsToAllowlist(Presence.OPTIONAL, 'track', kRTCMediaStreamTrackStats);

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
addRTCStatsToAllowlist(
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
  dtlsRole: 'string',
  srtpCipher: 'string',
  selectedCandidatePairChanges: 'number',
  iceRole: 'string',
  iceLocalUsernameFragment: 'string',
  iceState: 'string',
});
addRTCStatsToAllowlist(Presence.MANDATORY, 'transport', kRTCTransportStats);

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
  foundation: 'string',
  relatedAddress:  'string',
  relatedPort: 'number',
  usernameFragment: 'string',
  tcpType: 'string',
});
addRTCStatsToAllowlist(
    Presence.MANDATORY, 'local-candidate', kRTCIceCandidateStats);
addRTCStatsToAllowlist(
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
  packetsSent: 'number',
  packetsReceived: 'number',
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
  consentRequestsSent: 'number',
  packetsDiscardedOnSend: 'number',
  bytesDiscardedOnSend: 'number',
  lastPacketReceivedTimestamp: 'number',
  lastPacketSentTimestamp: 'number',
});
addRTCStatsToAllowlist(
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
addRTCStatsToAllowlist(Presence.MANDATORY, 'certificate', kRTCCertificateStats);

/*
 * RTCAudioPlayoutStats
 * https://w3c.github.io/webrtc-stats/#playoutstats-dict*
 * @private
 */
let kRTCAudioPlayoutStats = new RTCStats(null, {
  kind: 'string',
  synthesizedSamplesDuration: 'number',
  synthesizedSamplesEvents: 'number',
  totalSamplesDuration: 'number',
  totalPlayoutDelay: 'number',
  totalSamplesCount: 'number',
});
addRTCStatsToAllowlist(Presence.OPTIONAL, 'media-playout', kRTCAudioPlayoutStats);

// Public interface to tests. These are expected to be called with
// ExecuteJavascript invocations from the browser tests and will return answers
// through the DOM automation controller.

/**
 * Verifies that the promise-based |RTCPeerConnection.getStats| returns stats,
 * makes sure that all returned stats have the base RTCStats-members and that
 * all stats are allowed by the allowlist.
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
        verifyStatsIsAllowlisted_(stats);
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
 * Verifies that all stats types of the allowlist were contained in the result.
 *
 * Returns "ok-" followed by a double on success.
 */
function measureGetStatsPerformance() {
  let t0 = performance.now();
  peerConnection_().getStats()
    .then(function(report) {
      let t1 = performance.now();
      for (let stats of report.values()) {
        verifyStatsIsAllowlisted_(stats);
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
 * allowlist as a JSON-stringified array of strings to the test.
 */
function getMandatoryStatsTypes() {
  returnToTest(JSON.stringify(Array.from(mandatoryStatsTypes())));
}

// Internals.

/** Gets a set of all mandatory stats types. */
function mandatoryStatsTypes() {
  const mandatoryTypes = new Set();
  for (let allowlistedStats of gStatsAllowlist.values()) {
    if (allowlistedStats.presence() == Presence.MANDATORY)
      mandatoryTypes.add(allowlistedStats.type());
  }
  return mandatoryTypes;
}

/**
 * Checks if |stats| correctly maps to a a allowlisted RTCStats-derived
 * dictionary, throwing |failTest| if it doesn't. See |gStatsAllowlist|.
 *
 * The "RTCStats.type" must map to a known dictionary description. Every member
 * is optional, but if present it must be present in the allowlisted dictionary
 * description and its type must match.
 * @private
 */
function verifyStatsIsAllowlisted_(stats) {
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
  let allowlistedStats = gStatsAllowlist.get(stats.type);
  if (allowlistedStats == null)
    throw failTest('stats.type is not a allowlisted type: ' + stats.type);
  allowlistedStats = allowlistedStats.stats();
  for (let propertyName in stats) {
    if (propertyName === 'id' || propertyName === 'timestamp' ||
        propertyName === 'type') {
      continue;
    }
    if (!allowlistedStats.hasOwnProperty(propertyName)) {
      throw failTest(
          stats.type + '.' + propertyName + ' is not a allowlisted ' +
          'member: ' + stats[propertyName]);
    }
    if (allowlistedStats[propertyName] === 'any')
      continue;
    if (!allowlistedStats[propertyName].startsWith('sequence_')) {
      if (typeof(stats[propertyName]) !== allowlistedStats[propertyName]) {
        throw failTest('stats.' + propertyName + ' should have a different ' +
            'type according to the allowlist: ' + stats[propertyName] + ' vs ' +
            allowlistedStats[propertyName]);
      }
    } else {
      if (!Array.isArray(stats[propertyName])) {
        throw failTest('stats.' + propertyName + ' should have a different ' +
            'type according to the allowlist (should be an array): ' +
            JSON.stringify(stats[propertyName]) + ' vs ' +
            allowlistedStats[propertyName]);
      }
      let elementType = allowlistedStats[propertyName].substring(9);
      for (let element in stats[propertyName]) {
        if (typeof(element) !== elementType) {
          throw failTest('stats.' + propertyName + ' should have a different ' +
              'type according to the allowlist (an element of the array has ' +
              'the incorrect type): ' + JSON.stringify(stats[propertyName]) +
              ' vs ' + allowlistedStats[propertyName]);
        }
      }
    }
  }
}
