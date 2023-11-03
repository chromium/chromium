// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  forDebuggingOnly.reportAdAuctionWin(
    auctionConfig.sellerSignals.reportTo + '_debug_win_report');
  return bid;
}

function reportResult(
  auctionConfig, browserSignals) {
  sendReportTo(auctionConfig.sellerSignals.reportTo);
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
  };
}
