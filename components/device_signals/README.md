# Device Signals

This component contains the implementation of device signals (aka context-aware
signals) collection. It contains definition for services used to aggregate such
signals, along with utility functions that can be reused in various contexts.

A set of device signals can be used by Zero Trust access providers to assess a
device's security posture. That posture can then be used to calculate the risk
factor around granting access to restricted resources to the device (and its
user). This component facilitates the collection of those signals in Enterprise
use cases while also providing symbols for other use cases requiring some of
these signals (e.g. metrics).