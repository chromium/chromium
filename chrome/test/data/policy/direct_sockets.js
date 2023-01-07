'use strict';

async function mockTcp() {
  return typeof TCPSocket !== 'undefined';
}

async function mockUdp() {
  return typeof UDPSocket !== 'undefined';
}
