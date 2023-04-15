/**
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * See |setSdpDefaultCodec|.
 */
function setSdpDefaultAudioCodec(sdp, codec) {
  return setSdpDefaultCodec(
      sdp, 'audio', codec, false /* preferHwCodec */, null /* profile */);
}

/**
 * See |setSdpDefaultCodec|.
 */
function setSdpDefaultVideoCodec(sdp, codec, preferHwCodec, profile) {
  return setSdpDefaultCodec(sdp, 'video', codec, preferHwCodec, profile);
}

/**
 * Returns a modified version of |sdp| where the opus DTX flag has been
 * enabled.
 */
function setOpusDtxEnabled(sdp) {
  var sdpLines = splitSdpLines(sdp);

  // Get default audio codec
  var defaultCodec = getSdpDefaultAudioCodec(sdp);
  if (defaultCodec !== 'opus') {
    throw new MethodError('setOpusDtxEnabled',
            'Default audio codec is not set to \'opus\'.');
  }

  // Find codec ID for Opus, e.g. 111 if 'a=rtpmap:111 opus/48000/2'.
  var codecId = findRtpmapId(sdpLines, 'opus');
  if (codecId === null) {
    throw new MethodError(
      'setOpusDtxEnabled', 'Unknown ID for |codec| = \'opus\'.');
  }

  // Find 'a=fmtp:111' line, where 111 is the codecId
  var fmtLineNo = findFmtpLine(sdpLines, codecId);
  if (fmtLineNo === null) {
    // Add the line to the SDP.
    newLine = 'a=fmtp:' + codecId + ' usedtx=1'
    rtpMapLine = findRtpmapLine(sdpLines, codecId);
    sdpLines.splice(rtpMapLine + 1, 0, newLine);
  } else {
    // Modify the line to enable Opus Dtx.
    sdpLines[fmtLineNo] += ';usedtx=1'
  }
  return mergeSdpLines(sdpLines);
}

/**
 * See |setSdpTargetBitrate|.
 */
function setSdpVideoTargetBitrate(sdp, bitrate) {
  return setSdpTargetBitrate(sdp, 'video', bitrate);
}

/**
 * Returns a modified version of |sdp| where the |bitrate| has been set as
 * target, i.e. on the 'b=AS:|bitrate|' line, where |type| is 'audio' or
 * 'video'.
 * @private
 */
function setSdpTargetBitrate(sdp, type, bitrate) {
  var sdpLines = splitSdpLines(sdp);

  // Find 'm=|type|' line, e.g. 'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116'.
  var mLineNo = findLine(sdpLines, 'm=' + type);

  // Find 'b=AS:' line.
  var bLineNo = findLine(sdpLines, 'b=AS:' + type, mLineNo);
  if (bLineNo === null) {
    var cLineNo = findLine(sdpLines, 'c=', mLineNo) + 1;
    var newLines = sdpLines.slice(0, cLineNo)
    newLines.push("b=AS:" + bitrate);
    newLines = newLines.concat(sdpLines.slice(cLineNo, sdpLines.length));
    sdpLines = newLines;
  } else {
    sdpLines[bLineNo] = "b=AS:" + bitrate;
  }

  return mergeSdpLines(sdpLines);
}

/**
 * Returns a modified version of |sdp| where the |codec| with |profile| has been
 * promoted to be the default codec, i.e. the codec whose ID is first in the
 * list of codecs on the 'm=|type|' line, where |type| is 'audio' or 'video'.
 * @private
 */
function setSdpDefaultCodec(sdp, type, codec, preferHwCodec, profile) {
  var sdpLines = splitSdpLines(sdp);

  // Find codec ID, e.g. 100 for 'VP8' if 'a=rtpmap:100 VP8/9000'.
  // TODO(magjed): We need a more stable order of the video codecs, e.g. that HW
  // codecs are always listed before SW codecs.
  var useLastInstance = !preferHwCodec;
  var codecId = findRtpmapId(sdpLines, codec, useLastInstance, profile);
  if (codecId === null) {
    throw new MethodError(
        'setSdpDefaultCodec',
        'Unknown ID for |codec| = \'' + codec + '\' and |profile| = \'' +
            profile + '\'.');
  }

  // Find 'm=|type|' line, e.g. 'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116'.
  var mLineNo = findLine(sdpLines, 'm=' + type);
  if (mLineNo === null) {
    throw new MethodError('setSdpDefaultCodec',
            '\'m=' + type + '\' line missing from |sdp|.');
  }

  // Modify video line to use the desired codec as the default.
  sdpLines[mLineNo] = setMLineDefaultCodec(sdpLines[mLineNo], codecId);
  return mergeSdpLines(sdpLines);
}

/**
 * See |getSdpDefaultCodec|.
 */
function getSdpDefaultAudioCodec(sdp) {
  return getSdpDefaultCodec(sdp, 'audio');
}

/**
 * See |getSdpDefaultCodec|.
 */
function getSdpDefaultVideoCodec(sdp) {
  return getSdpDefaultCodec(sdp, 'video');
}

/**
 * Gets the default codec according to the |sdp|, i.e. the name of the codec
 * whose ID is first in the list of codecs on the 'm=|type|' line, where |type|
 * is 'audio' or 'video'.
 * @private
 */
function getSdpDefaultCodec(sdp, type) {
  var sdpLines = splitSdpLines(sdp);

  // Find 'm=|type|' line, e.g. 'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116'.
  var mLineNo = findLine(sdpLines, 'm=' + type);
  if (mLineNo === null) {
    throw new MethodError('getSdpDefaultCodec',
            '\'m=' + type + '\' line missing from |sdp|.');
  }

  // The default codec's ID.
  var defaultCodecId = getMLineDefaultCodec(sdpLines[mLineNo]);
  if (defaultCodecId === null) {
    throw new MethodError('getSdpDefaultCodec',
            '\'m=' + type + '\' line contains no codecs.');
  }

  // Find codec name, e.g. 'VP8' for 100 if 'a=rtpmap:100 VP8/9000'.
  var defaultCodec = findRtpmapCodec(sdpLines, defaultCodecId);
  if (defaultCodec === null) {
    throw new MethodError('getSdpDefaultCodec',
            'Unknown codec name for default codec ' + defaultCodecId + '.');
  }
  return defaultCodec;
}

/**
 * Searches through all |sdpLines| for the 'a=rtpmap:' line for the codec of
 * the specified name, returning its ID as an int if found, or null otherwise.
 * |codec| is the case-sensitive name of the codec. |profile| is the profile
 * specifier for the codec. If |lastInstance| is true, it will return the last
 * such ID, and if false, it will return the first such ID.
 * For example, if |sdpLines| contains 'a=rtpmap:100 VP8/9000' and |codec| is
 * 'VP8', this function returns 100.
 * @private
 */
function findRtpmapId(sdpLines, codec, lastInstance, profile) {
  var lineNo = findRtpmapLine(sdpLines, codec, lastInstance, profile);
  if (lineNo === null)
    return null;
  // Parse <id> from 'a=rtpmap:<id> <codec>/<rate>'.
  var id = sdpLines[lineNo].substring(9, sdpLines[lineNo].indexOf(' '));
  return parseInt(id);
}

/**
 * Searches through all |sdpLines| for the 'a=rtpmap:' line for the codec of
 * the specified codec ID, returning its name if found, or null otherwise.
 * For example, if |sdpLines| contains 'a=rtpmap:100 VP8/9000' and |id| is 100,
 * this function returns 'VP8'.
 * @private
 */
function findRtpmapCodec(sdpLines, id) {
  var lineNo = findRtpmapLine(sdpLines, id);
  if (lineNo === null)
    return null;
  // Parse <codec> from 'a=rtpmap:<id> <codec>/<rate>'.
  var from = sdpLines[lineNo].indexOf(' ');
  var to = sdpLines[lineNo].indexOf('/', from);
  if (from === null || to === null || from + 1 >= to)
    throw new MethodError('findRtpmapCodec', '');
  return sdpLines[lineNo].substring(from + 1, to);
}

/**
 * Finds a 'a=rtpmap:' line from |sdpLines| that contains |contains| (and
 * contains |optionalContains| in the following codec lines when given) and
 * returns its line index, or null if no such line was found. |contains| and
 * |optionalContains| may be the codec ID, codec name, bitrate or profile. If
 * |lastInstance| is true, it will return the last such line index, and if
 * false, it will return the first such line index. An 'a=rtpmap:' line looks
 * like this: 'a=rtpmap:<id> <codec>/<rate>'.
 */
function findRtpmapLine(sdpLines, contains, lastInstance, optionalContains) {
  if (lastInstance === true) {
    for (var i = sdpLines.length - 1; i >= 0 ; i--) {
      if (isRtpmapLineWithProfile(sdpLines, i, contains, optionalContains))
        return i;
    }
  } else {
    for (var i = 0; i < sdpLines.length; i++) {
      if (isRtpmapLineWithProfile(sdpLines, i, contains, optionalContains))
        return i;
    }
  }
  return null;
}

/**
 * Returns true if the given |lineIndex| is the start of a codec lines block
 * where sdpLines[lineIndex] satisfies the requirements for isRtpmapLine() and
 * there is a line following which contains |profileContains| if given.
 */
function isRtpmapLineWithProfile(
    sdpLines, lineIndex, contains, profileContains = undefined) {
  if (!isRtpmapLine(sdpLines[lineIndex], contains))
    return false;

  if (profileContains === null || profileContains === undefined)
    return true;
  var j = lineIndex + 1;
  while (j < sdpLines.length && sdpLines[j].indexOf('rtpmap:') == -1) {
    if (sdpLines[j].indexOf(profileContains) != -1)
      return true;
    j++;
  }
  return false;
}

/**
 * Returns true if |sdpLine| contains |contains| and is of pattern
 * 'a=rtpmap:<id> <codec>/<rate>'.
 */
function isRtpmapLine(sdpLine, contains) {
  // Is 'a=rtpmap:' line containing |contains| string?
  if (sdpLine.startsWith('a=rtpmap:') && sdpLine.indexOf(contains) != -1) {
    // Expecting pattern 'a=rtpmap:<id> <codec>/<rate>'.
    var pattern = new RegExp('a=rtpmap:(\\d+) \\w+\\/\\d+');
    if (!sdpLine.match(pattern))
      throw new MethodError('isRtpmapLine', 'Unexpected "a=rtpmap:" pattern.');
    return true;
  }
  return false;
}

/**
 * Finds the fmtp line in |sdpLines| for the given |codecId|, and returns its
 * line number. The line starts with 'a=fmtp:<codecId>'.
 * @private
 */
function findFmtpLine(sdpLines, codecId) {
  return findLine(sdpLines, 'a=fmtp:' + codecId);

}

/**
 * Returns a modified version of |mLine| that has |codecId| first in the list of
 * codec IDs. For example, setMLineDefaultCodec(
 *     'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116 117 96', 107)
 * Returns:
 *     'm=video 9 UDP/TLS/RTP/SAVPF 107 100 101 116 117 96'
 * @private
 */
function setMLineDefaultCodec(mLine, codecId) {
  var elements = mLine.split(' ');

  // Copy first three elements, codec order starts on fourth.
  var newLine = elements.slice(0, 3);

  // Put target |codecId| first and copy the rest.
  newLine.push(codecId);
  for (var i = 3; i < elements.length; i++) {
    if (elements[i] != codecId)
      newLine.push(elements[i]);
  }

  return newLine.join(' ');
}

/**
 * Returns the default codec's ID from the |mLine|, or null if the codec list is
 * empty. The default codec is the codec whose ID is first in the list of codec
 * IDs on the |mLine|. For example, getMLineDefaultCodec(
 *     'm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116 117 96')
 * Returns:
 *     100
 * @private
 */
function getMLineDefaultCodec(mLine) {
  var elements = mLine.split(' ');
  if (elements.length < 4)
    return null;
  return parseInt(elements[3]);
}

/** @private */
function splitSdpLines(sdp) {
  return sdp.split('\r\n');
}

/** @private */
function mergeSdpLines(sdpLines) {
  return sdpLines.join('\r\n');
}

/** @private */
function findLine(lines, lineStartsWith, startingLine = 0) {
  for (var i = startingLine; i < lines.length; i++) {
    if (lines[i].startsWith(lineStartsWith))
      return i;
  }
  return null;
}
