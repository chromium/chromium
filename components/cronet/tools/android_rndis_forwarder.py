# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import logging
import os
import re
import socket
import struct
import subprocess
import sys

# pylint: disable=wrong-import-position
REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools', 'perf'))
from core import path_util
sys.path.append(path_util.GetTelemetryDir())

from telemetry.core import platform
from telemetry.internal.platform import android_device
from telemetry.internal.util import binary_manager

from devil.android import device_errors
from devil.android import device_utils

import py_utils
# pylint: enable=wrong-import-position

# pylint: disable=useless-object-inheritance


class AndroidRndisForwarder(object):
  """Forwards traffic using RNDIS. Assumes the device has root access."""

  def __init__(self, device, rndis_configurator):
    self._device = device
    self._rndis_configurator = rndis_configurator
    self._device_iface = rndis_configurator.device_iface
    self._host_ip = rndis_configurator.host_ip
    self._original_dns = None, None, None
    self._RedirectPorts()
    # The netd commands fail on Lollipop and newer releases, but aren't
    # necessary as DNS isn't used.
    # self._OverrideDns()
    self._OverrideDefaultGateway()
    # Need to override routing policy again since call to setifdns
    # sometimes resets policy table
    self._rndis_configurator.OverrideRoutingPolicy()
    atexit.register(self.Close)
    # TODO(tonyg): Verify that each port can connect to host.

  @property
  def host_ip(self):
    return self._host_ip

  def Close(self):
    #if self._forwarding:
    #  self._rndis_configurator.RestoreRoutingPolicy()
    #  self._SetDns(*self._original_dns)
    #  self._RestoreDefaultGateway()
    #super(AndroidRndisForwarder, self).Close()
    pass

  def _RedirectPorts(self):
    """Sets the local to remote pair mappings to use for RNDIS."""
    # Flush any old nat rules.
    self._device.RunShellCommand(
        ['iptables', '-F', '-t', 'nat'], check_return=True)

  def _OverrideDns(self):
    """Overrides DNS on device to point at the host."""
    self._original_dns = self._GetCurrentDns()
    self._SetDns(self._device_iface, self.host_ip, self.host_ip)

  def _SetDns(self, iface, dns1, dns2):
    """Overrides device's DNS configuration.

    Args:
      iface: name of the network interface to make default
      dns1, dns2: nameserver IP addresses
    """
    if not iface:
      return  # If there is no route, then nobody cares about DNS.
    # DNS proxy in older versions of Android is configured via properties.
    # TODO(szym): run via su -c if necessary.
    self._device.SetProp('net.dns1', dns1)
    self._device.SetProp('net.dns2', dns2)
    dnschange = self._device.GetProp('net.dnschange')
    if dnschange:
      self._device.SetProp('net.dnschange', str(int(dnschange) + 1))
    # Since commit 8b47b3601f82f299bb8c135af0639b72b67230e6 to frameworks/base
    # the net.dns1 properties have been replaced with explicit commands for netd
    self._device.RunShellCommand(
        ['netd', 'resolver', 'setifdns', iface, dns1, dns2], check_return=True)
    # TODO(szym): if we know the package UID, we could setifaceforuidrange
    self._device.RunShellCommand(
        ['netd', 'resolver', 'setdefaultif', iface], check_return=True)

  def _GetCurrentDns(self):
    """Returns current gateway, dns1, and dns2."""
    routes = self._device.RunShellCommand(
        ['cat', '/proc/net/route'], check_return=True)[1:]
    routes = [route.split() for route in routes]
    default_routes = [route[0] for route in routes if route[1] == '00000000']
    return (
      default_routes[0] if default_routes else None,
      self._device.GetProp('net.dns1'),
      self._device.GetProp('net.dns2'),
    )

  def _OverrideDefaultGateway(self):
    """Force traffic to go through RNDIS interface.

    Override any default gateway route. Without this traffic may go through
    the wrong interface.

    This introduces the risk that _RestoreDefaultGateway() is not called
    (e.g. Telemetry crashes). A power cycle or "adb reboot" is a simple
    workaround around in that case.
    """
    # NOTE(pauljensen): On Nougat this can produce a weird message and return
    # a non-zero value, but routing still seems fine, so don't check_return.
    self._device.RunShellCommand(
        ['route', 'add', 'default', 'gw', self.host_ip,
         'dev', self._device_iface])

  def _RestoreDefaultGateway(self):
    self._device.RunShellCommand(
        ['netcfg', self._device_iface, 'down'], check_return=True)


class AndroidRndisConfigurator(object):
  """Configures a linux host to connect to an android device via RNDIS.

  Note that we intentionally leave RNDIS running on the device. This is
  because the setup is slow and potentially flaky and leaving it running
  doesn't seem to interfere with any other developer or bot use-cases.
  """

  _RNDIS_DEVICE = '/sys/class/android_usb/android0'
  _NETWORK_INTERFACES = '/etc/network/interfaces'
  _INTERFACES_INCLUDE = 'source /etc/network/interfaces.d/*.conf'
  _TELEMETRY_INTERFACE_FILE = '/etc/network/interfaces.d/telemetry-{}.conf'
  _DEVICE_IP_ADDRESS = '192.168.123.2'

  def __init__(self, device):
    self._device = device

    try:
      self._device.EnableRoot()
    except device_errors.CommandFailedError:
      logging.error('RNDIS forwarding requires a rooted device.')
      raise

    self._device_ip = None
    self._host_iface = None
    self._host_ip = None
    self.device_iface = None

    if platform.GetHostPlatform().GetOSName() == 'mac':
      self._InstallHorndis(platform.GetHostPlatform().GetArchName())

    assert self._IsRndisSupported(), 'Device does not support RNDIS.'
    self._CheckConfigureNetwork()

  @property
  def host_ip(self):
    return self._host_ip

  def _IsRndisSupported(self):
    """Checks that the device has RNDIS support in the kernel."""
    return self._device.FileExists('%s/f_rndis/device' % self._RNDIS_DEVICE)

  # pylint: disable=inconsistent-return-statements
  def _FindDeviceRndisInterface(self):
    """Returns the name of the RNDIS network interface if present."""
    config = self._device.RunShellCommand(
        ['ip', '-o', 'link', 'show'], check_return=True)
    interfaces = [line.split(':')[1].strip() for line in config]
    candidates = [iface for iface in interfaces if re.match('rndis|usb', iface)]
    if candidates:
      candidates.sort()
      if len(candidates) == 2 and candidates[0].startswith('rndis') and \
          candidates[1].startswith('usb'):
        return candidates[0]
      assert len(candidates) == 1, 'Found more than one rndis device!'
      return candidates[0]
  # pylint: enable=inconsistent-return-statements

  def _FindDeviceRndisMacAddress(self, interface):
    """Returns the MAC address of the RNDIS network interface if present."""
    config = self._device.RunShellCommand(
        ['ip', '-o', 'link', 'show', interface], check_return=True)[0]
    return config.split('link/ether ')[1][:17]

  def _EnumerateHostInterfaces(self):
    host_platform = platform.GetHostPlatform().GetOSName()
    if host_platform == 'linux':
      return subprocess.check_output(['ip', 'addr'],
                                     encoding='utf8').splitlines()
    if host_platform == 'mac':
      return subprocess.check_output(['ifconfig'], encoding='utf8').splitlines()
    raise NotImplementedError('Platform %s not supported!' % host_platform)

  # pylint: disable=inconsistent-return-statements
  def _FindHostRndisInterface(self, device_mac_address):
    """Returns the name of the host-side network interface."""
    interface_list = self._EnumerateHostInterfaces()
    ether_address = self._device.ReadFile(
        '%s/f_rndis/ethaddr' % self._RNDIS_DEVICE,
        as_root=True, force_pull=True).strip()
    interface_name = None
    for line in interface_list:
      if not line.startswith((' ', '\t')):
        interface_name = line.split(':')[-2].strip()
        # Attempt to ping device to trigger ARP for device.
        with open(os.devnull, 'wb') as devnull:
          subprocess.call(['ping', '-w1', '-c1', '-I', interface_name,
              self._DEVICE_IP_ADDRESS], stdout=devnull, stderr=devnull)
        # Check if ARP cache now has device in it.
        arp = subprocess.check_output(
            ['arp', '-i', interface_name, self._DEVICE_IP_ADDRESS],
            encoding='utf8')
        if device_mac_address in arp:
          return interface_name
      elif ether_address in line:
        return interface_name
      # NOTE(pauljensen): |ether_address| seems incorrect on Nougat devices,
      # but just going by the host interface name seems safe enough.
      elif interface_name == 'usb0':
        return interface_name
  # pylint: enable=inconsistent-return-statements

  def _WriteProtectedFile(self, file_path, contents):
    subprocess.check_call(
        ['/usr/bin/sudo', 'bash', '-c',
         'echo -e "%s" > %s' % (contents, file_path)])

  def _LoadInstalledHoRNDIS(self):
    """Attempt to load HoRNDIS if installed.
    If kext could not be loaded or if HoRNDIS is not installed, return False.
    """
    if not os.path.isdir('/System/Library/Extensions/HoRNDIS.kext'):
      logging.info('HoRNDIS not present on system.')
      return False

    def HoRNDISLoaded():
      return 'HoRNDIS' in subprocess.check_output(['kextstat'], encoding='utf8')

    if HoRNDISLoaded():
      return True

    logging.info('HoRNDIS installed but not running, trying to load manually.')
    subprocess.check_call(
        ['/usr/bin/sudo', 'kextload', '-b', 'com.joshuawise.kexts.HoRNDIS'])

    return HoRNDISLoaded()

  def _InstallHorndis(self, arch_name):
    if self._LoadInstalledHoRNDIS():
      logging.info('HoRNDIS kext loaded successfully.')
      return
    logging.info('Installing HoRNDIS...')
    pkg_path = binary_manager.FetchPath('horndis', 'mac', arch_name)
    subprocess.check_call(
        ['/usr/bin/sudo', 'installer', '-pkg', pkg_path, '-target', '/'])

  def _DisableRndis(self):
    # Set expect_status=None as this will temporarily break the adb connection.
    self._device.adb.Shell('setprop sys.usb.config adb', expect_status=None)
    self._device.WaitUntilFullyBooted()

  def _EnableRndis(self):
    """Enables the RNDIS network interface."""
    script_prefix = '/data/local/tmp/rndis'
    # This could be accomplished via "svc usb setFunction rndis" but only on
    # devices which have the "USB tethering" feature.
    # Also, on some devices, it's necessary to go through "none" function.
    script = """
trap '' HUP
trap '' TERM
trap '' PIPE

function manual_config() {
  echo %(functions)s > %(dev)s/functions
  echo 224 > %(dev)s/bDeviceClass
  echo 1 > %(dev)s/enable
  start adbd
  setprop sys.usb.state %(functions)s
}

# This function kills adb transport, so it has to be run "detached".
function doit() {
  setprop sys.usb.config none
  while [ `getprop sys.usb.state` != "none" ]; do
    sleep 1
  done
  manual_config
  # For some combinations of devices and host kernels, adb won't work unless the
  # interface is up, but if we bring it up immediately, it will break adb.
  #sleep 1
  if ip link show rndis0 ; then
    ifconfig rndis0 %(device_ip_address)s netmask 255.255.255.0 up
  else
    ifconfig usb0 %(device_ip_address)s netmask 255.255.255.0 up
  fi
  echo DONE >> %(prefix)s.log
}

doit &
    """ % {'dev': self._RNDIS_DEVICE,
           'functions': 'rndis,adb',
           'prefix': script_prefix,
           'device_ip_address': self._DEVICE_IP_ADDRESS}
    script_file = '%s.sh' % script_prefix
    log_file = '%s.log' % script_prefix
    self._device.WriteFile(script_file, script)
    # TODO(szym): run via su -c if necessary.
    self._device.RemovePath(log_file, force=True)
    self._device.RunShellCommand(['.', script_file], check_return=True)
    self._device.WaitUntilFullyBooted()
    result = self._device.ReadFile(log_file).splitlines()
    assert any('DONE' in line for line in result), 'RNDIS script did not run!'

  def _CheckEnableRndis(self, force):
    """Enables the RNDIS network interface, retrying if necessary.
    Args:
      force: Disable RNDIS first, even if it appears already enabled.
    Returns:
      device_iface: RNDIS interface name on the device
      host_iface: corresponding interface name on the host
    """
    for _ in range(3):
      if not force:
        device_iface = self._FindDeviceRndisInterface()
        if device_iface:
          device_mac_address = self._FindDeviceRndisMacAddress(device_iface)
          host_iface = self._FindHostRndisInterface(device_mac_address)
          if host_iface:
            return device_iface, host_iface
      self._DisableRndis()
      self._EnableRndis()
      force = False
    raise Exception('Could not enable RNDIS, giving up.')

  def _Ip2Long(self, addr):
    return struct.unpack('!L', socket.inet_aton(addr))[0]

  def _IpPrefix2AddressMask(self, addr):
    def _Length2Mask(length):
      return 0xFFFFFFFF & ~((1 << (32 - length)) - 1)

    addr, masklen = addr.split('/')
    return self._Ip2Long(addr), _Length2Mask(int(masklen))

  def _GetHostAddresses(self, iface):
    """Returns the IP addresses on host's interfaces, breaking out |iface|."""
    interface_list = self._EnumerateHostInterfaces()
    addresses = []
    iface_address = None
    found_iface = False
    for line in interface_list:
      if not line.startswith((' ', '\t')):
        found_iface = iface in line
      match = re.search(r'(?<=inet )\S+', line)
      if match:
        address = match.group(0)
        if '/' in address:
          address = self._IpPrefix2AddressMask(address)
        else:
          match = re.search(r'(?<=netmask )\S+', line)
          address = self._Ip2Long(address), int(match.group(0), 16)
        if found_iface:
          assert not iface_address, (
            'Found %s twice when parsing host interfaces.' % iface)
          iface_address = address
        else:
          addresses.append(address)
    return addresses, iface_address

  def _GetDeviceAddresses(self, excluded_iface):
    """Returns the IP addresses on all connected devices.
    Excludes interface |excluded_iface| on the selected device.
    """
    my_device = str(self._device)
    addresses = []
    for device_serial in android_device.GetDeviceSerials(None):
      try:
        device = device_utils.DeviceUtils(device_serial)
        if device_serial == my_device:
          excluded = excluded_iface
        else:
          excluded = 'no interfaces excluded on other devices'
        output = device.RunShellCommand(
            ['ip', '-o', '-4', 'addr'], check_return=True)
        addresses += [
            line.split()[3] for line in output if excluded not in line]
      except device_errors.CommandFailedError:
        logging.warning('Unable to determine IP addresses for %s',
                        device_serial)
    return addresses

  def _ConfigureNetwork(self, device_iface, host_iface):
    """Configures the |device_iface| to be on the same network as |host_iface|.
    """
    def _Long2Ip(value):
      return socket.inet_ntoa(struct.pack('!L', value))

    def _IsNetworkUnique(network, addresses):
      return all((addr & mask != network & mask) for addr, mask in addresses)

    # pylint: disable=inconsistent-return-statements
    def _NextUnusedAddress(network, netmask, used_addresses):
      # Excludes '0' and broadcast.
      for suffix in range(1, 0xFFFFFFFF & ~netmask):
        candidate = network | suffix
        if candidate not in used_addresses:
          return candidate
    # pylint: enable=inconsistent-return-statements

    def HasHostAddress():
      _, host_address = self._GetHostAddresses(host_iface)
      return bool(host_address)

    if not HasHostAddress():
      if platform.GetHostPlatform().GetOSName() == 'mac':
        if 'Telemetry' not in subprocess.check_output(
            ['networksetup', '-listallnetworkservices'], encoding='utf8'):
          subprocess.check_call(
              ['/usr/bin/sudo', 'networksetup',
               '-createnetworkservice', 'Telemetry', host_iface])
          subprocess.check_call(
              ['/usr/bin/sudo', 'networksetup',
               '-setmanual', 'Telemetry', '192.168.123.1', '255.255.255.0'])
      elif platform.GetHostPlatform().GetOSName() == 'linux':
        with open(self._NETWORK_INTERFACES) as f:
          orig_interfaces = f.read()
        if self._INTERFACES_INCLUDE not in orig_interfaces:
          interfaces = '\n'.join([
              orig_interfaces,
              '',
              '# Added by Telemetry.',
              self._INTERFACES_INCLUDE])
          self._WriteProtectedFile(self._NETWORK_INTERFACES, interfaces)
        interface_conf_file = self._TELEMETRY_INTERFACE_FILE.format(host_iface)
        if not os.path.exists(interface_conf_file):
          interface_conf_dir = os.path.dirname(interface_conf_file)
          if not os.path.exists(interface_conf_dir):
            subprocess.call(['/usr/bin/sudo', '/bin/mkdir', interface_conf_dir])
            subprocess.call(
                ['/usr/bin/sudo', '/bin/chmod', '755', interface_conf_dir])
          interface_conf = '\n'.join([
              '# Added by Telemetry for RNDIS forwarding.',
              'allow-hotplug %s' % host_iface,
              'iface %s inet static' % host_iface,
              '  address 192.168.123.1',
              '  netmask 255.255.255.0',
              ])
          self._WriteProtectedFile(interface_conf_file, interface_conf)
          subprocess.check_call(['/usr/bin/sudo', 'ifup', host_iface])
      logging.info('Waiting for RNDIS connectivity...')
      py_utils.WaitFor(HasHostAddress, 30)

    addresses, host_address = self._GetHostAddresses(host_iface)
    assert host_address, 'Interface %s could not be configured.' % host_iface

    host_ip, netmask = host_address  # pylint: disable=unpacking-non-sequence
    network = host_ip & netmask

    if not _IsNetworkUnique(network, addresses):
      logging.warning(
        'The IP address configuration %s of %s is not unique!\n'
        'Check your /etc/network/interfaces. If this overlap is intended,\n'
        'you might need to use: ip rule add from <device_ip> lookup <table>\n'
        'or add the interface to a bridge in order to route to this network.',
        host_address, host_iface
      )

    # Find unused IP address.
    used_addresses = [addr for addr, _ in addresses]
    used_addresses += [self._IpPrefix2AddressMask(addr)[0]
                       for addr in self._GetDeviceAddresses(device_iface)]
    used_addresses += [host_ip]

    device_ip = _NextUnusedAddress(network, netmask, used_addresses)
    assert device_ip, ('The network %s on %s is full.' %
                       (host_address, host_iface))

    host_ip = _Long2Ip(host_ip)
    device_ip = _Long2Ip(device_ip)
    netmask = _Long2Ip(netmask)

    # TODO(szym) run via su -c if necessary.
    self._device.RunShellCommand(
        ['ifconfig', device_iface, device_ip, 'netmask', netmask, 'up'],
        check_return=True)
    # Enabling the interface sometimes breaks adb.
    self._device.WaitUntilFullyBooted()
    self._host_iface = host_iface
    self._host_ip = host_ip
    self.device_iface = device_iface
    self._device_ip = device_ip

  def _TestConnectivity(self):
    with open(os.devnull, 'wb') as devnull:
      return subprocess.call(['ping', '-q', '-c1', '-W1', self._device_ip],
                             stdout=devnull) == 0

  def OverrideRoutingPolicy(self):
    """Override any routing policy that could prevent
    packets from reaching the rndis interface
    """
    policies = self._device.RunShellCommand(['ip', 'rule'], check_return=True)
    if len(policies) > 1 and not 'lookup main' in policies[1]:
      self._device.RunShellCommand(
          ['ip', 'rule', 'add', 'prio', '1', 'from', 'all', 'table', 'main'],
          check_return=True)
      self._device.RunShellCommand(
          ['ip', 'route', 'flush', 'cache'], check_return=True)

  def RestoreRoutingPolicy(self):
    policies = self._device.RunShellCommand(['ip', 'rule'], check_return=True)
    if len(policies) > 1 and re.match("^1:.*lookup main", policies[1]):
      self._device.RunShellCommand(
          ['ip', 'rule', 'del', 'prio', '1'], check_return=True)
      self._device.RunShellCommand(
          ['ip', 'route', 'flush', 'cache'], check_return=True)

  def _CheckConfigureNetwork(self):
    """Enables RNDIS and configures it, retrying until we have connectivity."""
    force = False
    for _ in range(3):
      device_iface, host_iface = self._CheckEnableRndis(force)
      self._ConfigureNetwork(device_iface, host_iface)
      self.OverrideRoutingPolicy()
      # Sometimes the first packet will wake up the connection.
      for _ in range(3):
        if self._TestConnectivity():
          return
      force = True
    self.RestoreRoutingPolicy()
    raise Exception('No connectivity, giving up.')
