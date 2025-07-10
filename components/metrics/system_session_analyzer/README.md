# System session analyzer
Queries the Windows system event log (viewable in the _Event Viewer_ app, under
_Windows Logs > System_) for startup and shutdown events to determine whether
the last shutdown was clean or unclean (i.e., a system-wide crash like a BSOD or
power outage).

Its purpose is to enable Chrome to distinguish browser crashes from total system
crashes, which may have nothing to do with Chrome.
