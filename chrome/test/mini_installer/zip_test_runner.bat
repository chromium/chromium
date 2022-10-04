@echo off

rem This batch file installs all Python packages needed for the test
rem and automatically runs the test. All args passed into this batch file
rem will be passed to run_mini_installer_tests.py. See
rem chrome\test\mini_installer\run_mini_installer_tests.py for the args that
rem can be passed.

python3 -m pip install --upgrade pip
python3 -m pip install psutil
python3 -m pip install requests
python3 -m pip install pypiwin32
python3 chrome\test\mini_installer\run_mini_installer_tests.py {run_args} %*
pause
