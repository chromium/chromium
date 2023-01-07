This directory contains lacros-specific D-Bus initialization, shutdown, and
utility code. Separation from //chromeos/lacros is necessary because this code
can call D-Bus managers, which in turn may depend on //chromeos/lacros (for
crosapi), whereas the the converse is disallowed.
