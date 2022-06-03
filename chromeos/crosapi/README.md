This directory defines the ChromeOS API (crosapi). This is the
communication protocol between lacros (web-browser on ChromeOS) and ash
(user-space system executable on ChromeOS) for all new IPCs. Some existing IPCs
might use Wayland or D-Bus to avoid unnecessary rewrites.

The ChromeOS API is eventually going to be stabilized and will need to tolerate
several milestones worth of version skew between lacros and ash. In the long
term, these interfaces will potentially need to support years of version skew.
This means that any mojom files and their transitive dependencies must be
relatively stable and backwards compatible. By default, mojom files in this
directory should not include mojoms from any other directories unless they are
marked \[Stable\].

Note: The mojom subdirectory contains the stable API. The cpp subdirectory holds
helper c++ code, but is not part of the API surface itself.