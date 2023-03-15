***OS Crypt***

This directory contains `OSCrypt` implementations that support cryptographic
primitives that allow binding data to the OS user.

There are two implementations, a [sync](sync) interface which can be called on
any thread, and an [async](async) interface that is instance based.

The [async](async) interface is currently under construction and so all current
usage should go via the [sync](sync) interface.

Please see the README.md in those directories for more information on each
implementation.
