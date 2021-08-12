# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import SimpleXMLRPCServer

import pywintypes
import servicemanager
import win32api
import win32service
import win32serviceutil


# TODO(crbug.com/1233612): Use portpick to choose an available port, and
# propagate the port to clients (for example, via a pre-defined registry key).
_XML_RPC_SERVER_PORT = 9090


class UpdaterTestRequestHanlder():

  # TODO(crbug.com/1233612): Replace this placeholder function with real ones to
  # serve test requests. Also consider to move this class into a separate
  # module.
  def echo(self, message):
    return message


class UpdaterTestXmlRpcServer(SimpleXMLRPCServer.SimpleXMLRPCServer):
  """Customized XML-RPC server for updater tests."""

  def run(self):
    """xml-rpc server main loop."""
    self.register_introspection_functions()
    self.register_instance(UpdaterTestRequestHanlder())
    self.serve_forever()


class UpdaterTestService(win32serviceutil.ServiceFramework):
  """Customizes updater tests behavior."""

  # Do not change these class variables names, these are required by the base
  # class.
  _svc_name_ = 'UpdaterTestService'
  _svc_display_name_ = 'Updater Test Service'
  _svc_description_ = 'Service for browser updater tests'


  def SvcStop(self):
    """Called by service framework to stop this service."""
    logging.info('Updater test service stopping...')
    self._xmlrpc_server.shutdown()
    self.ReportServiceStatus(win32service.SERVICE_STOPPED)

  def SvcDoRun(self):
    """Called by service framework to start this service."""

    try:
      logging.info('%s starting...', self._svc_name_)
      servicemanager.LogMsg(servicemanager.EVENTLOG_INFORMATION_TYPE,
                            servicemanager.PYS_SERVICE_STARTED,
                            (self._svc_name_, ''))
      self.ReportServiceStatus(win32service.SERVICE_RUNNING)
      self._xmlrpc_server = UpdaterTestXmlRpcServer(
          ('localhost', _XML_RPC_SERVER_PORT))
      self._xmlrpc_server.run()
      servicemanager.LogInfoMsg(self._svc_name_ + ' - Ended')
    except pywintypes.error as err:
      logging.exception(err)
      servicemanager.LogErrorMsg(err)
      self.ReportServiceStatus(win32service.SERVICE_ERROR_SEVERE)


if __name__ == "__main__":
  logging.info('Command: %s', sys.argv)

  # Prefer the pythonservice.exe in the same directory as the interpreter.
  # This is mainly for the vpython case.
  destination = os.path.join(
      os.path.dirname(os.path.abspath(sys.executable)), 'pythonservice.exe')
  if os.path.exists(destination):
    os.environ['PYTHON_SERVICE_EXE'] = destination

  try:
    win32api.SetConsoleCtrlHandler(lambda _: True, True)
    win32serviceutil.HandleCommandLine(UpdaterTestService)
  except Exception as err:
    servicemanager.LogErrorMsg(err)
